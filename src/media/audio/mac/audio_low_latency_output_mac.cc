// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_low_latency_output_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/media_switches.h"

namespace media {

static std::ostream& operator<<(std::ostream& os,
                                const AudioStreamBasicDescription& format) {
  os << "sample rate       : " << format.mSampleRate << std::endl
     << "format ID         : " << format.mFormatID << std::endl
     << "format flags      : " << format.mFormatFlags << std::endl
     << "bytes per packet  : " << format.mBytesPerPacket << std::endl
     << "frames per packet : " << format.mFramesPerPacket << std::endl
     << "bytes per frame   : " << format.mBytesPerFrame << std::endl
     << "channels per frame: " << format.mChannelsPerFrame << std::endl
     << "bits per channel  : " << format.mBitsPerChannel;
  return os;
}

static AudioObjectPropertyAddress kDefaultOutputDeviceAddress = {
  kAudioHardwarePropertyDefaultOutputDevice,
  kAudioObjectPropertyScopeGlobal,
  kAudioObjectPropertyElementMaster
};

// Overview of operation:
// 1) An object of AUAudioOutputStream is created by the AudioManager
// factory: audio_man->MakeAudioStream().
// 2) Next some thread will call Open(), at that point the underlying
// default output Audio Unit is created and configured.
// 3) Then some thread will call Start(source).
// Then the Audio Unit is started which creates its own thread which
// periodically will call the source for more data as buffers are being
// consumed.
// 4) At some point some thread will call Stop(), which we handle by directly
// stopping the default output Audio Unit.
// 6) The same thread that called stop will call Close() where we cleanup
// and notify the audio manager, which likely will destroy this object.

AUAudioOutputStream::AUAudioOutputStream(
    AudioManagerMac* manager, const AudioParameters& params)
    : manager_(manager),
      source_(NULL),
      output_unit_(0),
      output_device_id_(kAudioObjectUnknown),
      volume_(1),
      hardware_latency_frames_(0),
      stopped_(false),
      audio_bus_(AudioBus::Create(params)) {
  // We must have a manager.
  DCHECK(manager_);

  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = params.bits_per_sample();
  format_.mChannelsPerFrame = params.channels();
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = (format_.mBitsPerChannel * params.channels()) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
  format_.mReserved = 0;

  DVLOG(1) << "Desired ouput format: " << format_;

  // Calculate the number of sample frames per callback.
  number_of_frames_ = params.GetBytesPerBuffer() / format_.mBytesPerPacket;
  DVLOG(1) << "Number of frames per callback: " << number_of_frames_;
  CHECK_EQ(number_of_frames_, GetAudioHardwareBufferSize());
}

AUAudioOutputStream::~AUAudioOutputStream() {
}

bool AUAudioOutputStream::Open() {
  // Obtain the current input device selected by the user.
  UInt32 size = sizeof(output_device_id_);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &kDefaultOutputDeviceAddress,
                                               0,
                                               0,
                                               &size,
                                               &output_device_id_);
  if (result != noErr || output_device_id_ == kAudioObjectUnknown) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default audio output device.";
    return false;
  }

  // Open and initialize the DefaultOutputUnit.
  AudioComponent comp;
  AudioComponentDescription desc;

  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  comp = AudioComponentFindNext(0, &desc);
  if (!comp)
    return false;

  result = AudioComponentInstanceNew(comp, &output_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "AudioComponentInstanceNew() failed.";
    return false;
  }

  result = AudioUnitInitialize(output_unit_);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "AudioUnitInitialize() failed.";
    return false;
  }

  hardware_latency_frames_ = GetHardwareLatency();

  return Configure();
}

bool AUAudioOutputStream::Configure() {
  // Set the render callback.
  AURenderCallbackStruct input;
  input.inputProc = InputProc;
  input.inputProcRefCon = this;
  OSStatus result = AudioUnitSetProperty(
      output_unit_,
      kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Global,
      0,
      &input,
      sizeof(input));
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
      << "AudioUnitSetProperty(kAudioUnitProperty_SetRenderCallback) failed.";
    return false;
  }

  // Set the stream format.
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Input,
      0,
      &format_,
      sizeof(format_));
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed.";
    return false;
  }

  // Set the buffer frame size.
  // WARNING: Setting this value changes the frame size for all audio units in
  // the current process.  It's imperative that the input and output frame sizes
  // be the same as audio_util::GetAudioHardwareBufferSize().
  // See http://crbug.com/154352 for details.
  UInt32 buffer_size = number_of_frames_;
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioDevicePropertyBufferFrameSize,
      kAudioUnitScope_Output,
      0,
      &buffer_size,
      sizeof(buffer_size));
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }

  return true;
}

void AUAudioOutputStream::Close() {
  if (output_unit_)
    AudioComponentInstanceDispose(output_unit_);

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AUAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  if (!output_unit_) {
    DLOG(ERROR) << "Open() has not been called successfully";
    return;
  }

  stopped_ = false;
  source_ = callback;

  AudioOutputUnitStart(output_unit_);
}

void AUAudioOutputStream::Stop() {
  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  if (stopped_)
    return;

  AudioOutputUnitStop(output_unit_);

  source_ = NULL;
  stopped_ = true;
}

void AUAudioOutputStream::SetVolume(double volume) {
  if (!output_unit_)
    return;
  volume_ = static_cast<float>(volume);

  // TODO(crogers): set volume property
}

void AUAudioOutputStream::GetVolume(double* volume) {
  if (!output_unit_)
    return;
  *volume = volume_;
}

// Pulls on our provider to get rendered audio stream.
// Note to future hackers of this function: Do not add locks here because this
// is running on a real-time thread (for low-latency).
OSStatus AUAudioOutputStream::Render(UInt32 number_of_frames,
                                     AudioBufferList* io_data,
                                     const AudioTimeStamp* output_time_stamp) {
  // Update the playout latency.
  double playout_latency_frames = GetPlayoutLatency(output_time_stamp);

  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8* audio_data = reinterpret_cast<uint8*>(buffer.mData);
  uint32 hardware_pending_bytes = static_cast<uint32>
      ((playout_latency_frames + 0.5) * format_.mBytesPerFrame);

  // Unfortunately AUAudioInputStream and AUAudioOutputStream share the frame
  // size set by kAudioDevicePropertyBufferFrameSize above on a per process
  // basis.  What this means is that the |number_of_frames| value may be larger
  // or smaller than the value set during Configure().  In this case either
  // audio input or audio output will be broken, so just output silence.
  // TODO(crogers): Figure out what can trigger a change in |number_of_frames|.
  // See http://crbug.com/1543 for details.
   if (number_of_frames != static_cast<UInt32>(audio_bus_->frames())) {
     memset(audio_data, 0, number_of_frames * format_.mBytesPerFrame);
     return noErr;
   }

  int frames_filled = source_->OnMoreData(
      audio_bus_.get(), AudioBuffersState(0, hardware_pending_bytes));

  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  audio_bus_->ToInterleaved(
      frames_filled, format_.mBitsPerChannel / 8, audio_data);
  uint32 filled = frames_filled * format_.mBytesPerFrame;

  // Perform in-place, software-volume adjustments.
  media::AdjustVolume(audio_data,
                      filled,
                      audio_bus_->channels(),
                      format_.mBitsPerChannel / 8,
                      volume_);

  return noErr;
}

// DefaultOutputUnit callback
OSStatus AUAudioOutputStream::InputProc(void* user_data,
                                        AudioUnitRenderActionFlags*,
                                        const AudioTimeStamp* output_time_stamp,
                                        UInt32,
                                        UInt32 number_of_frames,
                                        AudioBufferList* io_data) {
  AUAudioOutputStream* audio_output =
      static_cast<AUAudioOutputStream*>(user_data);
  if (!audio_output)
    return -1;

  return audio_output->Render(number_of_frames, io_data, output_time_stamp);
}

int AUAudioOutputStream::HardwareSampleRate() {
  // Determine the default output device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  UInt32 info_size = sizeof(device_id);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &kDefaultOutputDeviceAddress,
                                               0,
                                               0,
                                               &info_size,
                                               &device_id);
  if (result != noErr || device_id == kAudioObjectUnknown) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default audio output device.";
    return 0;
  }

  Float64 nominal_sample_rate;
  info_size = sizeof(nominal_sample_rate);

  AudioObjectPropertyAddress nominal_sample_rate_address = {
      kAudioDevicePropertyNominalSampleRate,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
  };
  result = AudioObjectGetPropertyData(device_id,
                                      &nominal_sample_rate_address,
                                      0,
                                      0,
                                      &info_size,
                                      &nominal_sample_rate);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default sample rate for device: " << device_id;
    return 0;
  }

  return static_cast<int>(nominal_sample_rate);
}

double AUAudioOutputStream::GetHardwareLatency() {
  if (!output_unit_ || output_device_id_ == kAudioObjectUnknown) {
    DLOG(WARNING) << "Audio unit object is NULL or device ID is unknown";
    return 0.0;
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(output_unit_,
                                         kAudioUnitProperty_Latency,
                                         kAudioUnitScope_Global,
                                         0,
                                         &audio_unit_latency_sec,
                                         &size);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get audio unit latency";
    return 0.0;
  }

  // Get output audio device latency.
  AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyLatency,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };
  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(output_device_id_,
                                      &property_address,
                                      0,
                                      NULL,
                                      &size,
                                      &device_latency_frames);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result) << "Could not get audio unit latency";
    return 0.0;
  }

  return static_cast<double>((audio_unit_latency_sec *
      format_.mSampleRate) + device_latency_frames);
}

double AUAudioOutputStream::GetPlayoutLatency(
    const AudioTimeStamp* output_time_stamp) {
  // Ensure mHostTime is valid.
  if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0)
    return 0;

  // Get the delay between the moment getting the callback and the scheduled
  // time stamp that tells when the data is going to be played out.
  UInt64 output_time_ns = AudioConvertHostTimeToNanos(
      output_time_stamp->mHostTime);
  UInt64 now_ns = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

  // Prevent overflow leading to huge delay information; occurs regularly on
  // the bots, probably less so in the wild.
  if (now_ns > output_time_ns)
    return 0;

  double delay_frames = static_cast<double>
      (1e-9 * (output_time_ns - now_ns) * format_.mSampleRate);

  return (delay_frames + hardware_latency_frames_);
}

}  // namespace media
