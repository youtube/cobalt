// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"

#include <CoreAudio/AudioHardware.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#include "media/audio/mac/audio_output_mac.h"
#include "media/audio/mac/audio_synchronized_mac.h"
#include "media/audio/mac/audio_unified_mac.h"
#include "media/base/bind_to_loop.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
    selector,
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  UInt32 output_device_id_size = static_cast<UInt32>(sizeof(output_device_id));
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
  return err == kAudioHardwareNoError &&
      output_device_id != kAudioObjectUnknown;
}

// Returns true if the default input device is the same as
// the default output device.
static bool HasUnifiedDefaultIO() {
  AudioDeviceID input_id, output_id;

  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioHardwarePropertyDefaultInputDevice;
  pa.mScope = kAudioObjectPropertyScopeGlobal;
  pa.mElement = kAudioObjectPropertyElementMaster;
  UInt32 size = sizeof(input_id);

  // Get the default input.
  OSStatus result = AudioObjectGetPropertyData(
      kAudioObjectSystemObject,
      &pa,
      0,
      0,
      &size,
      &input_id);

  if (result != noErr)
    return false;

  // Get the default output.
  pa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
  result = AudioObjectGetPropertyData(
      kAudioObjectSystemObject,
      &pa,
      0,
      0,
      &size,
      &output_id);

  if (result != noErr)
    return false;

  return input_id == output_id;
}

static void GetAudioDeviceInfo(bool is_input,
                               media::AudioDeviceNames* device_names) {
  DCHECK(device_names);
  device_names->clear();

  // Query the number of total devices.
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                   &property_address,
                                                   0,
                                                   NULL,
                                                   &size);
  if (result || !size)
    return;

  int device_count = size / sizeof(AudioDeviceID);

  // Get the array of device ids for all the devices, which includes both
  // input devices and output devices.
  scoped_ptr_malloc<AudioDeviceID>
      devices(reinterpret_cast<AudioDeviceID*>(malloc(size)));
  AudioDeviceID* device_ids = devices.get();
  result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &property_address,
                                      0,
                                      NULL,
                                      &size,
                                      device_ids);
  if (result)
    return;

  // Iterate over all available devices to gather information.
  for (int i = 0; i < device_count; ++i) {
    // Get the number of input or output channels of the device.
    property_address.mScope = is_input ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    property_address.mSelector = kAudioDevicePropertyStreams;
    size = 0;
    result = AudioObjectGetPropertyDataSize(device_ids[i],
                                            &property_address,
                                            0,
                                            NULL,
                                            &size);
    if (result || !size)
      continue;

    // Get device UID.
    CFStringRef uid = NULL;
    size = sizeof(uid);
    property_address.mSelector = kAudioDevicePropertyDeviceUID;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    result = AudioObjectGetPropertyData(device_ids[i],
                                        &property_address,
                                        0,
                                        NULL,
                                        &size,
                                        &uid);
    if (result)
      continue;

    // Get device name.
    CFStringRef name = NULL;
    property_address.mSelector = kAudioObjectPropertyName;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    result = AudioObjectGetPropertyData(device_ids[i],
                                        &property_address,
                                        0,
                                        NULL,
                                        &size,
                                        &name);
    if (result) {
      if (uid)
        CFRelease(uid);
      continue;
    }

    // Store the device name and UID.
    media::AudioDeviceName device_name;
    device_name.device_name = base::SysCFStringRefToUTF8(name);
    device_name.unique_id = base::SysCFStringRefToUTF8(uid);
    device_names->push_back(device_name);

    // We are responsible for releasing the returned CFObject.  See the
    // comment in the AudioHardware.h for constant
    // kAudioDevicePropertyDeviceUID.
    if (uid)
      CFRelease(uid);
    if (name)
      CFRelease(name);
  }
}

static AudioDeviceID GetAudioDeviceIdByUId(bool is_input,
                                           const std::string& device_id) {
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  AudioDeviceID audio_device_id = kAudioObjectUnknown;
  UInt32 device_size = sizeof(audio_device_id);
  OSStatus result = -1;

  if (device_id == AudioManagerBase::kDefaultDeviceId) {
    // Default Device.
    property_address.mSelector = is_input ?
        kAudioHardwarePropertyDefaultInputDevice :
        kAudioHardwarePropertyDefaultOutputDevice;

    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &device_size,
                                        &audio_device_id);
  } else {
    // Non-default device.
    base::mac::ScopedCFTypeRef<CFStringRef>
        uid(base::SysUTF8ToCFStringRef(device_id));
    AudioValueTranslation value;
    value.mInputData = &uid;
    value.mInputDataSize = sizeof(CFStringRef);
    value.mOutputData = &audio_device_id;
    value.mOutputDataSize = device_size;
    UInt32 translation_size = sizeof(AudioValueTranslation);

    property_address.mSelector = kAudioHardwarePropertyDeviceForUID;
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &translation_size,
                                        &value);
  }

  if (result) {
    OSSTATUS_DLOG(WARNING, result) << "Unable to query device " << device_id
                                   << " for AudioDeviceID";
  }

  return audio_device_id;
}

// Property address to monitor for device changes.
static const AudioObjectPropertyAddress kDeviceChangePropertyAddress = {
  kAudioHardwarePropertyDefaultOutputDevice,
  kAudioObjectPropertyScopeGlobal,
  kAudioObjectPropertyElementMaster
};

// Callback from the system when the default device changes; this must be called
// on the MessageLoop that created the AudioManager.
static OSStatus OnDefaultDeviceChangedCallback(
    AudioObjectID object,
    UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[],
    void* context) {
  if (object != kAudioObjectSystemObject)
    return noErr;

  for (UInt32 i = 0; i < num_addresses; ++i) {
    if (addresses[i].mSelector == kDeviceChangePropertyAddress.mSelector &&
        addresses[i].mScope == kDeviceChangePropertyAddress.mScope &&
        addresses[i].mElement == kDeviceChangePropertyAddress.mElement &&
        context) {
      static_cast<AudioManagerMac*>(context)->OnDeviceChange();
      break;
    }
  }

  return noErr;
}

AudioManagerMac::AudioManagerMac()
    : listener_registered_(false),
      creating_message_loop_(base::MessageLoopProxy::current()) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // AudioManagerMac is expected to be created by the root platform thread, this
  // is generally BrowserMainLoop, it's MessageLoop will drive the NSApplication
  // pump which in turn fires the property listener callbacks.
  if (!creating_message_loop_)
    return;

  OSStatus result = AudioObjectAddPropertyListener(
      kAudioObjectSystemObject,
      &kDeviceChangePropertyAddress,
      &OnDefaultDeviceChangedCallback,
      this);

  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioObjectAddPropertyListener() failed!";
    return;
  }

  listener_registered_ = true;
}

AudioManagerMac::~AudioManagerMac() {
  if (listener_registered_) {
    // TODO(dalecurtis): CHECK destruction happens on |creating_message_loop_|,
    // should be true, but currently several unit tests perform destruction in
    // odd places so we can't CHECK here currently.
    OSStatus result = AudioObjectRemovePropertyListener(
        kAudioObjectSystemObject,
        &kDeviceChangePropertyAddress,
        &OnDefaultDeviceChangedCallback,
        this);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "AudioObjectRemovePropertyListener() failed!";
  }

  Shutdown();
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

void AudioManagerMac::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  GetAudioDeviceInfo(true, device_names);
  if (!device_names->empty()) {
    // Prepend the default device to the list since we always want it to be
    // on the top of the list for all platforms. There is no duplicate
    // counting here since the default device has been abstracted out before.
    media::AudioDeviceName name;
    name.device_name = AudioManagerBase::kDefaultDeviceName;
    name.unique_id =  AudioManagerBase::kDefaultDeviceId;
    device_names->push_front(name);
  }
}

AudioOutputStream* AudioManagerMac::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new PCMQueueOutAudioOutputStream(this, params);
}

AudioOutputStream* AudioManagerMac::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  // TODO(crogers): remove once we properly handle input device selection.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAudioInput)) {
    if (HasUnifiedDefaultIO())
      return new AudioHardwareUnifiedStream(this, params);

    // kAudioDeviceUnknown translates to "use default" here.
    return new AudioSynchronizedStream(this,
                                       params,
                                       kAudioDeviceUnknown,
                                       kAudioDeviceUnknown);
  }

  return new AUAudioOutputStream(this, params);
}

AudioInputStream* AudioManagerMac::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new PCMQueueInAudioInputStream(this, params);
}

AudioInputStream* AudioManagerMac::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // Gets the AudioDeviceID that refers to the AudioOutputDevice with the device
  // unique id. This AudioDeviceID is used to set the device for Audio Unit.
  AudioDeviceID audio_device_id = GetAudioDeviceIdByUId(true, device_id);
  AudioInputStream* stream = NULL;
  if (audio_device_id != kAudioObjectUnknown)
    stream = new AUAudioInputStream(this, params, audio_device_id);

  return stream;
}

AudioParameters AudioManagerMac::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAudioInput)) {
    // TODO(crogers): given the limitations of the AudioOutputStream
    // back-ends used with kEnableWebAudioInput, we hard-code to stereo.
    // Specifically, this is a limitation of AudioSynchronizedStream which
    // can be removed as part of the work to consolidate these back-ends.
    return AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        GetAudioHardwareSampleRate(), 16, GetAudioHardwareBufferSize());
  }

  return AudioManagerBase::GetPreferredLowLatencyOutputStreamParameters(
      input_params);
}

void AudioManagerMac::OnDeviceChange() {
  // Post the task to the |creating_message_loop_| to execute our listener
  // callback.  The callback is created using BindToLoop() so will hop over
  // to the audio thread upon execution.
  creating_message_loop_->PostTask(FROM_HERE, BindToLoop(
      GetMessageLoop(), base::Bind(
          &AudioManagerMac::NotifyAllOutputDeviceChangeListeners,
          base::Unretained(this))));
}

AudioManager* CreateAudioManager() {
  return new AudioManagerMac();
}

}  // namespace media
