// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"
#include "media/base/limits.h"

static const int kMaxInputChannels = 2;

// Maximum number of output streams that can be open simultaneously.
static const size_t kMaxOutputStreams = 50;

// By experiment the maximum number of audio streams allowed in Leopard
// is 18. But we put a slightly smaller number just to be safe.
static const size_t kMaxOutputStreamsLeopard = 15;

// Initialized to ether |kMaxOutputStreams| or |kMaxOutputStreamsLeopard|.
static size_t g_max_output_streams = 0;

// Returns the number of audio streams allowed. This is a practical limit to
// prevent failure caused by too many audio streams opened.
static size_t GetMaxAudioOutputStreamsAllowed() {
  if (g_max_output_streams == 0) {
    // We are hitting a bug in Leopard where too many audio streams will cause
    // a deadlock in the AudioQueue API when starting the stream. Unfortunately
    // there's no way to detect it within the AudioQueue API, so we put a
    // special hard limit only for Leopard.
    // See bug: http://crbug.com/30242
    if (base::mac::IsOSLeopardOrEarlier()) {
      g_max_output_streams = kMaxOutputStreamsLeopard;
    } else {
      // In OS other than OSX Leopard, the number of audio streams
      // allowed is a lot more.
      g_max_output_streams = kMaxOutputStreams;
    }
  }

  return g_max_output_streams;
}

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
    selector,
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  size_t output_device_id_size = sizeof(output_device_id);
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
  return err == kAudioHardwareNoError &&
      output_device_id != kAudioObjectUnknown;
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
    int channels = 0;
    // Get the number of input or output channels of the device.
    property_address.mScope = is_input ?
        kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    property_address.mSelector = kAudioDevicePropertyStreamConfiguration;
    result = AudioObjectGetPropertyDataSize(device_ids[i],
                                            &property_address,
                                            0,
                                            NULL,
                                            &size);
    if (result)
      continue;

    scoped_ptr_malloc<AudioBufferList>
        buffer(reinterpret_cast<AudioBufferList*>(malloc(size)));
    AudioBufferList* buffer_list = buffer.get();
    result = AudioObjectGetPropertyData(device_ids[i],
                                        &property_address,
                                        0,
                                        NULL,
                                        &size,
                                        buffer_list);
    if (result)
      continue;

    for (uint32 j = 0; j < buffer_list->mNumberBuffers; ++j)
      channels += buffer_list->mBuffers[j].mNumberChannels;

    // Exclude those devices without the type of channel we are interested in.
    if (!channels)
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
    DLOG(WARNING) << "Unable to query device " << device_id
                  << " for AudioDeviceID ";
  }

  return audio_device_id;
}

AudioManagerMac::AudioManagerMac()
    : num_output_streams_(0) {
}

AudioManagerMac::~AudioManagerMac() {
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

void AudioManagerMac::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  // This is needed because AudioObjectGetPropertyDataSize has memory leak
  // when there is no soundcard in the machine.
  if (!HasAudioInputDevices())
    return;

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

AudioOutputStream* AudioManagerMac::MakeAudioOutputStream(
    const AudioParameters& params) {
  if (!params.IsValid())
    return NULL;

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242
  if (num_output_streams_ >= GetMaxAudioOutputStreamsAllowed()) {
    return NULL;
  }

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream(params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    num_output_streams_++;
    return new PCMQueueOutAudioOutputStream(this, params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    num_output_streams_++;
    return new AUAudioOutputStream(this, params);
  }
  return NULL;
}

AudioInputStream* AudioManagerMac::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  if (!params.IsValid() || (params.channels > kMaxInputChannels) ||
      device_id.empty())
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    return new PCMQueueInAudioInputStream(this, params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    // Gets the AudioDeviceID that refers to the AudioDevice with the device
    // unique id. This AudioDeviceID is used to set the device for Audio Unit.
    AudioDeviceID audio_device_id = GetAudioDeviceIdByUId(true, device_id);
    if (audio_device_id != kAudioObjectUnknown)
      return new AUAudioInputStream(this, params, audio_device_id);
  }
  return NULL;
}

void AudioManagerMac::MuteAll() {
  // TODO(cpu): implement.
}

void AudioManagerMac::UnMuteAll() {
  // TODO(cpu): implement.
}

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(stream);
  num_output_streams_--;
  delete stream;
}

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseInputStream(AudioInputStream* stream) {
  delete stream;
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerMac();
}
