// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_unified_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

namespace media {

// TODO(crogers): support more than hard-coded stereo input.
// Ideally we would like to receive this value as a constructor argument.
static const int kDefaultInputChannels = 2;

AudioHardwareUnifiedStream::AudioHardwareUnifiedStream(
    AudioManagerMac* manager, const AudioParameters& params)
    : manager_(manager),
      source_(NULL),
      client_input_channels_(kDefaultInputChannels),
      volume_(1.0f),
      input_channels_(0),
      output_channels_(0),
      input_channels_per_frame_(0),
      output_channels_per_frame_(0),
      io_proc_id_(0),
      device_(kAudioObjectUnknown),
      is_playing_(false) {
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

  // Calculate the number of sample frames per callback.
  number_of_frames_ = params.GetBytesPerBuffer() / format_.mBytesPerPacket;

  input_bus_ = AudioBus::Create(client_input_channels_,
                                params.frames_per_buffer());
  output_bus_ = AudioBus::Create(params);
}

AudioHardwareUnifiedStream::~AudioHardwareUnifiedStream() {
  DCHECK_EQ(device_, kAudioObjectUnknown);
}

bool AudioHardwareUnifiedStream::Open() {
  // Obtain the current output device selected by the user.
  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
  pa.mScope = kAudioObjectPropertyScopeGlobal;
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size = sizeof(device_);

  OSStatus result = AudioObjectGetPropertyData(
      kAudioObjectSystemObject,
      &pa,
      0,
      0,
      &size,
      &device_);

  if ((result != kAudioHardwareNoError) || (device_ == kAudioDeviceUnknown)) {
    LOG(ERROR) << "Cannot open unified AudioDevice.";
    return false;
  }

  // The requested sample-rate must match the hardware sample-rate.
  Float64 sample_rate = 0.0;
  size = sizeof(sample_rate);

  pa.mSelector = kAudioDevicePropertyNominalSampleRate;
  pa.mScope = kAudioObjectPropertyScopeWildcard;
  pa.mElement = kAudioObjectPropertyElementMaster;

  result = AudioObjectGetPropertyData(
      device_,
      &pa,
      0,
      0,
      &size,
      &sample_rate);

  if (result != noErr || sample_rate != format_.mSampleRate) {
    LOG(ERROR) << "Requested sample-rate: " << format_.mSampleRate
        <<  " must match the hardware sample-rate: " << sample_rate;
    return false;
  }

  // Configure buffer frame size.
  UInt32 frame_size = number_of_frames_;

  pa.mSelector = kAudioDevicePropertyBufferFrameSize;
  pa.mScope = kAudioDevicePropertyScopeInput;
  pa.mElement = kAudioObjectPropertyElementMaster;
  result = AudioObjectSetPropertyData(
      device_,
      &pa,
      0,
      0,
      sizeof(frame_size),
      &frame_size);

  if (result != noErr) {
    LOG(ERROR) << "Unable to set input buffer frame size: "  << frame_size;
    return false;
  }

  pa.mScope = kAudioDevicePropertyScopeOutput;
  result = AudioObjectSetPropertyData(
      device_,
      &pa,
      0,
      0,
      sizeof(frame_size),
      &frame_size);

  if (result != noErr) {
    LOG(ERROR) << "Unable to set output buffer frame size: "  << frame_size;
    return false;
  }

  DVLOG(1) << "Sample rate: " << sample_rate;
  DVLOG(1) << "Frame size: " << frame_size;

  // Determine the number of input and output channels.
  // We handle both the interleaved and non-interleaved cases.

  // Get input stream configuration.
  pa.mSelector = kAudioDevicePropertyStreamConfiguration;
  pa.mScope = kAudioDevicePropertyScopeInput;
  pa.mElement = kAudioObjectPropertyElementMaster;

  result = AudioObjectGetPropertyDataSize(device_, &pa, 0, 0, &size);
  OSSTATUS_DCHECK(result == noErr, result);

  if (result == noErr && size > 0) {
    // Allocate storage.
    scoped_array<uint8> input_list_storage(new uint8[size]);
    AudioBufferList& input_list =
        *reinterpret_cast<AudioBufferList*>(input_list_storage.get());

    result = AudioObjectGetPropertyData(
        device_,
        &pa,
        0,
        0,
        &size,
        &input_list);
    OSSTATUS_DCHECK(result == noErr, result);

    if (result == noErr) {
      // Determine number of input channels.
      input_channels_per_frame_ = input_list.mNumberBuffers > 0 ?
          input_list.mBuffers[0].mNumberChannels : 0;
      if (input_channels_per_frame_ == 1 && input_list.mNumberBuffers > 1) {
        // Non-interleaved.
        input_channels_ = input_list.mNumberBuffers;
      } else {
        // Interleaved.
        input_channels_ = input_channels_per_frame_;
      }
    }
  }

  DVLOG(1) << "Input channels: " << input_channels_;
  DVLOG(1) << "Input channels per frame: " << input_channels_per_frame_;

  // The hardware must have at least the requested input channels.
  if (result != noErr || client_input_channels_ > input_channels_) {
    LOG(ERROR) << "AudioDevice does not support requested input channels.";
    return false;
  }

  // Get output stream configuration.
  pa.mSelector = kAudioDevicePropertyStreamConfiguration;
  pa.mScope = kAudioDevicePropertyScopeOutput;
  pa.mElement = kAudioObjectPropertyElementMaster;

  result = AudioObjectGetPropertyDataSize(device_, &pa, 0, 0, &size);
  OSSTATUS_DCHECK(result == noErr, result);

  if (result == noErr && size > 0) {
    // Allocate storage.
    scoped_array<uint8> output_list_storage(new uint8[size]);
    AudioBufferList& output_list =
        *reinterpret_cast<AudioBufferList*>(output_list_storage.get());

    result = AudioObjectGetPropertyData(
        device_,
        &pa,
        0,
        0,
        &size,
        &output_list);
    OSSTATUS_DCHECK(result == noErr, result);

    if (result == noErr) {
      // Determine number of output channels.
      output_channels_per_frame_ = output_list.mBuffers[0].mNumberChannels;
      if (output_channels_per_frame_ == 1 && output_list.mNumberBuffers > 1) {
        // Non-interleaved.
        output_channels_ = output_list.mNumberBuffers;
      } else {
        // Interleaved.
        output_channels_ = output_channels_per_frame_;
      }
    }
  }

  DVLOG(1) << "Output channels: " << output_channels_;
  DVLOG(1) << "Output channels per frame: " << output_channels_per_frame_;

  // The hardware must have at least the requested output channels.
  if (result != noErr ||
      output_channels_ < static_cast<int>(format_.mChannelsPerFrame)) {
    LOG(ERROR) << "AudioDevice does not support requested output channels.";
    return false;
  }

  // Setup the I/O proc.
  result = AudioDeviceCreateIOProcID(device_, RenderProc, this, &io_proc_id_);
  if (result != noErr) {
    LOG(ERROR) << "Error creating IOProc.";
    return false;
  }

  return true;
}

void AudioHardwareUnifiedStream::Close() {
  DCHECK(!is_playing_);

  OSStatus result = AudioDeviceDestroyIOProcID(device_, io_proc_id_);
  OSSTATUS_DCHECK(result == noErr, result);

  io_proc_id_ = 0;
  device_ = kAudioObjectUnknown;

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AudioHardwareUnifiedStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  DCHECK_NE(device_, kAudioObjectUnknown);
  DCHECK(!is_playing_);
  if (device_ == kAudioObjectUnknown || is_playing_)
    return;

  source_ = callback;

  OSStatus result = AudioDeviceStart(device_, io_proc_id_);
  OSSTATUS_DCHECK(result == noErr, result);

  if (result == noErr)
    is_playing_ = true;
}

void AudioHardwareUnifiedStream::Stop() {
  if (!is_playing_)
    return;

  source_ = NULL;

  if (device_ != kAudioObjectUnknown) {
    OSStatus result = AudioDeviceStop(device_, io_proc_id_);
    OSSTATUS_DCHECK(result == noErr, result);
  }

  is_playing_ = false;
}

void AudioHardwareUnifiedStream::SetVolume(double volume) {
  volume_ = static_cast<float>(volume);
  // TODO(crogers): set volume property
}

void AudioHardwareUnifiedStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Pulls on our provider with optional input, asking it to render output.
// Note to future hackers of this function: Do not add locks here because this
// is running on a real-time thread (for low-latency).
OSStatus AudioHardwareUnifiedStream::Render(
    AudioDeviceID device,
    const AudioTimeStamp* now,
    const AudioBufferList* input_data,
    const AudioTimeStamp* input_time,
    AudioBufferList* output_data,
    const AudioTimeStamp* output_time) {
  // Convert the input data accounting for possible interleaving.
  // TODO(crogers): it's better to simply memcpy() if source is already planar.
  if (input_channels_ >= client_input_channels_) {
    for (int channel_index = 0; channel_index < client_input_channels_;
         ++channel_index) {
      float* source;

      int source_channel_index = channel_index;

      if (input_channels_per_frame_ > 1) {
        // Interleaved.
        source = static_cast<float*>(input_data->mBuffers[0].mData) +
            source_channel_index;
      } else {
        // Non-interleaved.
        source = static_cast<float*>(
            input_data->mBuffers[source_channel_index].mData);
      }

      float* p = input_bus_->channel(channel_index);
      for (int i = 0; i < number_of_frames_; ++i) {
        p[i] = *source;
        source += input_channels_per_frame_;
      }
    }
  } else if (input_channels_) {
    input_bus_->Zero();
  }

  // Give the client optional input data and have it render the output data.
  source_->OnMoreIOData(input_bus_.get(),
                        output_bus_.get(),
                        AudioBuffersState(0, 0));

  // TODO(crogers): handle final Core Audio 5.1 layout for 5.1 audio.

  // Handle interleaving as necessary.
  // TODO(crogers): it's better to simply memcpy() if dest is already planar.

  for (int channel_index = 0;
       channel_index < static_cast<int>(format_.mChannelsPerFrame);
       ++channel_index) {
    float* dest;

    int dest_channel_index = channel_index;

    if (output_channels_per_frame_ > 1) {
      // Interleaved.
      dest = static_cast<float*>(output_data->mBuffers[0].mData) +
          dest_channel_index;
    } else {
      // Non-interleaved.
      dest = static_cast<float*>(
          output_data->mBuffers[dest_channel_index].mData);
    }

    float* p = output_bus_->channel(channel_index);
    for (int i = 0; i < number_of_frames_; ++i) {
      *dest = p[i];
      dest += output_channels_per_frame_;
    }
  }

  return noErr;
}

OSStatus AudioHardwareUnifiedStream::RenderProc(
    AudioDeviceID device,
    const AudioTimeStamp* now,
    const AudioBufferList* input_data,
    const AudioTimeStamp* input_time,
    AudioBufferList* output_data,
    const AudioTimeStamp* output_time,
    void* user_data) {
  AudioHardwareUnifiedStream* audio_output =
      static_cast<AudioHardwareUnifiedStream*>(user_data);
  DCHECK(audio_output);
  if (!audio_output)
    return -1;

  return audio_output->Render(
      device,
      now,
      input_data,
      input_time,
      output_data,
      output_time);
}

}  // namespace media
