// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_low_latency_output_mac.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

using media::SwizzleCoreAudioLayout5_1;

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
    AudioManagerMac* manager, AudioParameters params)
    : manager_(manager),
      source_(NULL),
      output_unit_(0),
      volume_(1) {
  // We must have a manager.
  DCHECK(manager_);
  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = params.sample_rate;
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = params.bits_per_sample;
  format_.mChannelsPerFrame = params.channels;
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = (format_.mBitsPerChannel * params.channels) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;

  // Calculate the number of sample frames per callback.
  number_of_frames_ = params.GetPacketSize() / format_.mBytesPerPacket;
}

AUAudioOutputStream::~AUAudioOutputStream() {
}

bool AUAudioOutputStream::Open() {
  // Open and initialize the DefaultOutputUnit.
  Component comp;
  ComponentDescription desc;

  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  comp = FindNextComponent(0, &desc);
  DCHECK(comp);

  OSStatus result = OpenAComponent(comp, &output_unit_);
  DCHECK_EQ(result, 0);
  if (result)
    return false;

  result = AudioUnitInitialize(output_unit_);

  DCHECK_EQ(result, 0);
  if (result)
    return false;

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

  DCHECK_EQ(result, 0);
  if (result)
    return false;

  // Set the stream format.
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Input,
      0,
      &format_,
      sizeof(format_));
  DCHECK_EQ(result, 0);
  if (result)
    return false;

  // Set the buffer frame size.
  UInt32 buffer_size = number_of_frames_;
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioDevicePropertyBufferFrameSize,
      kAudioUnitScope_Output,
      0,
      &buffer_size,
      sizeof(buffer_size));
  DCHECK_EQ(result, 0);
  if (result)
    return false;

  return true;
}

void AUAudioOutputStream::Close() {
  if (output_unit_)
    CloseComponent(output_unit_);

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AUAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  source_ = callback;

  AudioOutputUnitStart(output_unit_);
}

void AUAudioOutputStream::Stop() {
  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  source_ = NULL;

  AudioOutputUnitStop(output_unit_);
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
                                     AudioBufferList* io_data) {
  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8* audio_data = reinterpret_cast<uint8*>(buffer.mData);
  uint32 filled = source_->OnMoreData(
      this, audio_data, buffer.mDataByteSize, AudioBuffersState(0, 0));

  // Handle channel order for 5.1 audio.
  if (format_.mChannelsPerFrame == 6) {
    if (format_.mBitsPerChannel == 8) {
      SwizzleCoreAudioLayout5_1(reinterpret_cast<uint8*>(audio_data), filled);
    } else if (format_.mBitsPerChannel == 16) {
      SwizzleCoreAudioLayout5_1(reinterpret_cast<int16*>(audio_data), filled);
    } else if (format_.mBitsPerChannel == 32) {
      SwizzleCoreAudioLayout5_1(reinterpret_cast<int32*>(audio_data), filled);
    }
  }

  return noErr;
}

// DefaultOutputUnit callback
OSStatus AUAudioOutputStream::InputProc(void* user_data,
                                        AudioUnitRenderActionFlags*,
                                        const AudioTimeStamp*,
                                        UInt32,
                                        UInt32 number_of_frames,
                                        AudioBufferList* io_data) {
  AUAudioOutputStream* audio_output =
      static_cast<AUAudioOutputStream*>(user_data);
  DCHECK(audio_output);
  if (!audio_output)
    return -1;

  return audio_output->Render(number_of_frames, io_data);
}

double AUAudioOutputStream::HardwareSampleRate() {
  // Determine the default output device's sample-rate.
  AudioDeviceID device_id = kAudioDeviceUnknown;
  UInt32 info_size = sizeof(device_id);

  AudioObjectPropertyAddress default_input_device_address = {
      kAudioHardwarePropertyDefaultInputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &default_input_device_address,
                                               0,
                                               0,
                                               &info_size,
                                               &device_id);
  DCHECK_EQ(result, 0);
  if (result)
    return 0.0;  // error

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
  DCHECK_EQ(result, 0);
  if (result)
    return 0.0;  // error

  return nominal_sample_rate;
}
