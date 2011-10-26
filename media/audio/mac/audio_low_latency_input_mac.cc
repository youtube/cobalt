// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_low_latency_input_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

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

// See "Technical Note TN2091 - Device input using the HAL Output Audio Unit"
// http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
// for more details and background regarding this implementation.

AUAudioInputStream::AUAudioInputStream(
    AudioManagerMac* manager, const AudioParameters& params)
    : manager_(manager),
      sink_(NULL),
      audio_unit_(0),
      started_(false) {
  DCHECK(manager_);

  // Set up the desired (output) format specified by the client.
  format_.mSampleRate = params.sample_rate;
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = params.bits_per_sample;
  format_.mChannelsPerFrame = params.channels;
  format_.mFramesPerPacket = 1;  // uncompressed audio
  format_.mBytesPerPacket = (format_.mBitsPerChannel *
                             params.channels) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
  format_.mReserved = 0;

  DVLOG(1) << "Desired ouput format: " << format_;

  // Calculate the number of sample frames per callback.
  number_of_frames_ = params.GetPacketSize() / format_.mBytesPerPacket;
  DVLOG(1) << "Number of frames per callback: " << number_of_frames_;

  // Derive size (in bytes) of the buffers that we will render to.
  UInt32 data_byte_size = number_of_frames_ * format_.mBytesPerFrame;
  DVLOG(1) << "Size of data buffer in bytes : " << data_byte_size;

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer.
  audio_data_buffer_.reset(new uint8[data_byte_size]);
  audio_buffer_list_.mNumberBuffers = 1;

  AudioBuffer* audio_buffer = audio_buffer_list_.mBuffers;
  audio_buffer->mNumberChannels = params.channels;
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = audio_data_buffer_.get();
}

AUAudioInputStream::~AUAudioInputStream() {}

// Obtain and open the AUHAL AudioOutputUnit for recording.
bool AUAudioInputStream::Open() {
  // Verify that we are not already opened.
  if (audio_unit_)
    return false;

  // Start by obtaining an AudioOuputUnit using an AUHAL component description.

  Component comp;
  ComponentDescription desc;

  // Description for the Audio Unit we want to use (AUHAL in this case).
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_HALOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  comp = FindNextComponent(0, &desc);
  DCHECK(comp);

  // Get access to the service provided by the specified Audio Unit.
  OSStatus result = OpenAComponent(comp, &audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // Enable IO on the input scope of the Audio Unit.

  // After creating the AUHAL object, we must enable IO on the input scope
  // of the Audio Unit to obtain the device input. Input must be explicitly
  // enabled with the kAudioOutputUnitProperty_EnableIO property on Element 1
  // of the AUHAL. Beacause the AUHAL can be used for both input and output,
  // we must also disable IO on the output scope.

  UInt32 enableIO = 1;

  // Enable input on the AUHAL.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                1,          // input element 1
                                &enableIO,  // enable
                                sizeof(enableIO));
  if (result) {
    HandleError(result);
    return false;
  }

  // Disable output on the AUHAL.
  enableIO = 0;
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                0,          // output element 0
                                &enableIO,  // disable
                                sizeof(enableIO));
  if (result) {
    HandleError(result);
    return false;
  }

  // Set the current device of the AudioOuputUnit to default input device.

  AudioDeviceID input_device;
  UInt32 size = sizeof(input_device);

  // First, obtain the current input device selected by the user.
  result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                    &size,
                                    &input_device);
  if (result) {
    HandleError(result);
    return false;
  }

  // Next, set the audio device to be the Audio Unit's current device.
  // Note that, devices can only be set to the AUHAL after enabling IO.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global,
                                0,
                                &input_device,
                                sizeof(input_device));
  if (result) {
    HandleError(result);
    return false;
  }

  // Register the input procedure for the AUHAL.
  // This procedure will be called when the AUHAL has received new data
  // from the input device.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioOutputUnitProperty_SetInputCallback,
                                kAudioUnitScope_Global,
                                0,
                                &callback,
                                sizeof(callback));
  if (result) {
    HandleError(result);
    return false;
  }

  // Set up the the desired (output) format.
  // For obtaining input from a device, the device format is always expressed
  // on the output scope of the AUHAL's Element 1.
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output,
                                1,
                                &format_,
                                sizeof(format_));
  if (result) {
    HandleError(result);
    return false;
  }

  // Set the desired number of frames in the IO buffer (output scope).
  result = AudioUnitSetProperty(audio_unit_,
                                kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Output,
                                1,
                                &number_of_frames_,  // size is set in the ctor
                                sizeof(number_of_frames_));
  if (result) {
    HandleError(result);
    return false;
  }

  // Finally, initialize the audio unit and ensure that it is ready to render.
  // Allocates memory according to the maximum number of audio frames
  // it can produce in response to a single render call.
  result = AudioUnitInitialize(audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }
  return true;
}

void AUAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  if (started_)
    return;
  sink_ = callback;
  OSStatus result = AudioOutputUnitStart(audio_unit_);
  if (result == noErr) {
    started_ = true;
  }
  DLOG_IF(ERROR, result != noErr) << "Failed to start acquiring data";
}

void AUAudioInputStream::Stop() {
  if (!started_)
    return;
  OSStatus result;
  result = AudioOutputUnitStop(audio_unit_);
  if (result == noErr) {
    started_ = false;
  }
  DLOG_IF(ERROR, result != noErr) << "Failed to stop acquiring data";
}

void AUAudioInputStream::Close() {
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  if (started_) {
    Stop();
  }
  if (audio_unit_) {
    // Deallocate the audio unit’s resources.
    AudioUnitUninitialize(audio_unit_);

    // Terminates our connection to the AUHAL component.
    CloseComponent(audio_unit_);
    audio_unit_ = 0;
  }
  if (sink_) {
    sink_->OnClose(this);
    sink_ = NULL;
  }

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

// AUHAL AudioDeviceOutput unit callback
OSStatus AUAudioInputStream::InputProc(void* user_data,
                                       AudioUnitRenderActionFlags* flags,
                                       const AudioTimeStamp* time_stamp,
                                       UInt32 bus_number,
                                       UInt32 number_of_frames,
                                       AudioBufferList* io_data) {
  // Verify that the correct bus is used (Input bus/Element 1)
  DCHECK_EQ(bus_number, static_cast<UInt32>(1));
  AUAudioInputStream* audio_input =
      reinterpret_cast<AUAudioInputStream*>(user_data);
  DCHECK(audio_input);
  if (!audio_input)
    return kAudioUnitErr_InvalidElement;

  // Receive audio from the AUHAL from the output scope of the Audio Unit.
  OSStatus result = AudioUnitRender(audio_input->audio_unit(),
                                    flags,
                                    time_stamp,
                                    bus_number,
                                    number_of_frames,
                                    audio_input->audio_buffer_list());
  if (result)
    return result;

  // Deliver recorded data to the consumer as a callback.
  return audio_input->Provide(number_of_frames,
                              audio_input->audio_buffer_list());
}

OSStatus AUAudioInputStream::Provide(UInt32 number_of_frames,
                                     AudioBufferList* io_data) {
  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8* audio_data = reinterpret_cast<uint8*>(buffer.mData);
  DCHECK(audio_data);
  if (!audio_data)
    return kAudioUnitErr_InvalidElement;

  // TODO(henrika): improve delay estimation. Using buffer size for now.
  sink_->OnData(this, audio_data, buffer.mDataByteSize, buffer.mDataByteSize);

  return noErr;
}

double AUAudioInputStream::HardwareSampleRate() {
  // Determine the default input device's sample-rate.
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
    return 0.0;

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
    return 0.0;

  return nominal_sample_rate;
}

void AUAudioInputStream::HandleError(OSStatus err) {
  NOTREACHED() << "error code: " << err;
  if (sink_)
    sink_->OnError(this, static_cast<int>(err));
}
