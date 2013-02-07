// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_synchronized_mac.h"

#include <CoreServices/CoreServices.h>
#include <algorithm>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

namespace media {

static const int kHardwareBufferSize = 128;
static const int kFifoSize = 16384;

// TODO(crogers): handle the non-stereo case.
static const int kChannels = 2;

// This value was determined empirically for minimum latency while still
// guarding against FIFO under-runs.
static const int kBaseTargetFifoFrames = 256 + 64;

// If the input and output sample-rate don't match, then we need to maintain
// an additional safety margin due to the callback timing jitter and the
// varispeed buffering.  This value was empirically tuned.
static const int kAdditionalTargetFifoFrames = 128;

static void ZeroBufferList(AudioBufferList* buffer_list) {
  for (size_t i = 0; i < buffer_list->mNumberBuffers; ++i)
    memset(buffer_list->mBuffers[i].mData,
           0,
           buffer_list->mBuffers[i].mDataByteSize);
}

static void WrapBufferList(AudioBufferList* buffer_list,
                           AudioBus* bus,
                           int frames) {
  DCHECK(buffer_list);
  DCHECK(bus);
  int channels = bus->channels();
  int buffer_list_channels = buffer_list->mNumberBuffers;

  // Copy pointers from AudioBufferList.
  int source_idx = 0;
  for (int i = 0; i < channels; ++i) {
    bus->SetChannelData(
        i, static_cast<float*>(buffer_list->mBuffers[source_idx].mData));

    // It's ok to pass in a |buffer_list| with fewer channels, in which
    // case we just duplicate the last channel.
    if (source_idx < buffer_list_channels - 1)
      ++source_idx;
  }

  // Finally set the actual length.
  bus->set_frames(frames);
}

AudioSynchronizedStream::AudioSynchronizedStream(
    AudioManagerMac* manager,
    const AudioParameters& params,
    AudioDeviceID input_id,
    AudioDeviceID output_id)
    : manager_(manager),
      params_(params),
      input_sample_rate_(0),
      output_sample_rate_(0),
      input_id_(input_id),
      output_id_(output_id),
      input_buffer_list_(NULL),
      fifo_(kChannels, kFifoSize),
      target_fifo_frames_(kBaseTargetFifoFrames),
      average_delta_(0.0),
      fifo_rate_compensation_(1.0),
      input_unit_(0),
      varispeed_unit_(0),
      output_unit_(0),
      first_input_time_(-1),
      is_running_(false),
      hardware_buffer_size_(kHardwareBufferSize),
      channels_(kChannels) {
}

AudioSynchronizedStream::~AudioSynchronizedStream() {
  DCHECK(!input_unit_);
  DCHECK(!output_unit_);
  DCHECK(!varispeed_unit_);
}

bool AudioSynchronizedStream::Open() {
  if (params_.channels() != kChannels) {
    LOG(ERROR) << "Only stereo output is currently supported.";
    return false;
  }

  // Create the input, output, and varispeed AudioUnits.
  OSStatus result = CreateAudioUnits();
  if (result != noErr) {
    LOG(ERROR) << "Cannot create AudioUnits.";
    return false;
  }

  result = SetupInput(input_id_);
  if (result != noErr) {
    LOG(ERROR) << "Error configuring input AudioUnit.";
    return false;
  }

  result = SetupOutput(output_id_);
  if (result != noErr) {
    LOG(ERROR) << "Error configuring output AudioUnit.";
    return false;
  }

  result = SetupCallbacks();
  if (result != noErr) {
    LOG(ERROR) << "Error setting up callbacks on AudioUnits.";
    return false;
  }

  result = SetupStreamFormats();
  if (result != noErr) {
    LOG(ERROR) << "Error configuring stream formats on AudioUnits.";
    return false;
  }

  AllocateInputData();

  // Final initialization of the AudioUnits.
  result = AudioUnitInitialize(input_unit_);
  if (result != noErr) {
    LOG(ERROR) << "Error initializing input AudioUnit.";
    return false;
  }

  result = AudioUnitInitialize(output_unit_);
  if (result != noErr) {
    LOG(ERROR) << "Error initializing output AudioUnit.";
    return false;
  }

  result = AudioUnitInitialize(varispeed_unit_);
  if (result != noErr) {
    LOG(ERROR) << "Error initializing varispeed AudioUnit.";
    return false;
  }

  if (input_sample_rate_ != output_sample_rate_) {
    // Add extra safety margin.
    target_fifo_frames_ += kAdditionalTargetFifoFrames;
  }

  // Buffer initial silence corresponding to target I/O buffering.
  fifo_.Clear();
  scoped_ptr<AudioBus> silence =
      AudioBus::Create(channels_, target_fifo_frames_);
  silence->Zero();
  fifo_.Push(silence.get());

  return true;
}

void AudioSynchronizedStream::Close() {
  DCHECK(!is_running_);

  if (input_buffer_list_) {
    free(input_buffer_list_);
    input_buffer_list_ = 0;
    input_bus_.reset(NULL);
    wrapper_bus_.reset(NULL);
  }

  if (input_unit_) {
    AudioUnitUninitialize(input_unit_);
    CloseComponent(input_unit_);
  }

  if (output_unit_) {
    AudioUnitUninitialize(output_unit_);
    CloseComponent(output_unit_);
  }

  if (varispeed_unit_) {
    AudioUnitUninitialize(varispeed_unit_);
    CloseComponent(varispeed_unit_);
  }

  input_unit_ = NULL;
  output_unit_ = NULL;
  varispeed_unit_ = NULL;

  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void AudioSynchronizedStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  DCHECK(input_unit_);
  DCHECK(output_unit_);
  DCHECK(varispeed_unit_);

  if (is_running_ || !input_unit_ || !output_unit_ || !varispeed_unit_)
    return;

  source_ = callback;

  // Reset state variables each time we Start().
  fifo_rate_compensation_ = 1.0;
  average_delta_ = 0.0;

  OSStatus result = noErr;

  if (!is_running_) {
    first_input_time_ = -1;

    result = AudioOutputUnitStart(input_unit_);
    OSSTATUS_DCHECK(result == noErr, result);

    if (result == noErr) {
      result = AudioOutputUnitStart(output_unit_);
      OSSTATUS_DCHECK(result == noErr, result);
    }
  }

  is_running_ = true;
}

void AudioSynchronizedStream::Stop() {
  OSStatus result = noErr;
  if (is_running_) {
    result = AudioOutputUnitStop(input_unit_);
    OSSTATUS_DCHECK(result == noErr, result);

    if (result == noErr) {
      result = AudioOutputUnitStop(output_unit_);
      OSSTATUS_DCHECK(result == noErr, result);
    }
  }

  if (result == noErr)
    is_running_ = false;
}

bool AudioSynchronizedStream::IsRunning() {
  return is_running_;
}

// TODO(crogers): implement - or remove from AudioOutputStream.
void AudioSynchronizedStream::SetVolume(double volume) {}
void AudioSynchronizedStream::GetVolume(double* volume) {}

OSStatus AudioSynchronizedStream::SetOutputDeviceAsCurrent(
    AudioDeviceID output_id) {
  OSStatus result = noErr;

  // Get the default output device if device is unknown.
  if (output_id == kAudioDeviceUnknown) {
    AudioObjectPropertyAddress pa;
    pa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    pa.mScope = kAudioObjectPropertyScopeGlobal;
    pa.mElement = kAudioObjectPropertyElementMaster;
    UInt32 size = sizeof(output_id);

    result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &pa,
        0,
        0,
        &size,
        &output_id);

    OSSTATUS_DCHECK(result == noErr, result);
    if (result != noErr)
      return result;
  }

  // Set the render frame size.
  UInt32 frame_size = hardware_buffer_size_;
  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyBufferFrameSize;
  pa.mScope = kAudioDevicePropertyScopeInput;
  pa.mElement = kAudioObjectPropertyElementMaster;
  result = AudioObjectSetPropertyData(
      output_id,
      &pa,
      0,
      0,
      sizeof(frame_size),
      &frame_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  output_info_.Initialize(output_id, false);

  // Set the Current Device to the Default Output Unit.
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global,
      0,
      &output_info_.id_,
      sizeof(output_info_.id_));

  OSSTATUS_DCHECK(result == noErr, result);
  return result;
}

OSStatus AudioSynchronizedStream::SetInputDeviceAsCurrent(
    AudioDeviceID input_id) {
  OSStatus result = noErr;

  // Get the default input device if device is unknown.
  if (input_id == kAudioDeviceUnknown) {
    AudioObjectPropertyAddress pa;
    pa.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    pa.mScope = kAudioObjectPropertyScopeGlobal;
    pa.mElement = kAudioObjectPropertyElementMaster;
    UInt32 size = sizeof(input_id);

    result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &pa,
        0,
        0,
        &size,
        &input_id);

    OSSTATUS_DCHECK(result == noErr, result);
    if (result != noErr)
      return result;
  }

  // Set the render frame size.
  UInt32 frame_size = hardware_buffer_size_;
  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyBufferFrameSize;
  pa.mScope = kAudioDevicePropertyScopeInput;
  pa.mElement = kAudioObjectPropertyElementMaster;
  result = AudioObjectSetPropertyData(
      input_id,
      &pa,
      0,
      0,
      sizeof(frame_size),
      &frame_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  input_info_.Initialize(input_id, true);

  // Set the Current Device to the AUHAL.
  // This should be done only after I/O has been enabled on the AUHAL.
  result = AudioUnitSetProperty(
      input_unit_,
      kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global,
      0,
      &input_info_.id_,
      sizeof(input_info_.id_));

  OSSTATUS_DCHECK(result == noErr, result);
  return result;
}

OSStatus AudioSynchronizedStream::CreateAudioUnits() {
  // Q: Why do we need a varispeed unit?
  // A: If the input device and the output device are running at
  // different sample rates and/or on different clocks, we will need
  // to compensate to avoid a pitch change and
  // to avoid buffer under and over runs.
  ComponentDescription varispeed_desc;
  varispeed_desc.componentType = kAudioUnitType_FormatConverter;
  varispeed_desc.componentSubType = kAudioUnitSubType_Varispeed;
  varispeed_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  varispeed_desc.componentFlags = 0;
  varispeed_desc.componentFlagsMask = 0;

  Component varispeed_comp = FindNextComponent(NULL, &varispeed_desc);
  if (varispeed_comp == NULL)
    return -1;

  OSStatus result = OpenAComponent(varispeed_comp, &varispeed_unit_);
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Open input AudioUnit.
  ComponentDescription input_desc;
  input_desc.componentType = kAudioUnitType_Output;
  input_desc.componentSubType = kAudioUnitSubType_HALOutput;
  input_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  input_desc.componentFlags = 0;
  input_desc.componentFlagsMask = 0;

  Component input_comp = FindNextComponent(NULL, &input_desc);
  if (input_comp == NULL)
    return -1;

  result = OpenAComponent(input_comp, &input_unit_);
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Open output AudioUnit.
  ComponentDescription output_desc;
  output_desc.componentType = kAudioUnitType_Output;
  output_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  output_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  output_desc.componentFlags = 0;
  output_desc.componentFlagsMask = 0;

  Component output_comp = FindNextComponent(NULL, &output_desc);
  if (output_comp == NULL)
    return -1;

  result = OpenAComponent(output_comp, &output_unit_);
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  return noErr;
}

OSStatus AudioSynchronizedStream::SetupInput(AudioDeviceID input_id) {
  // The AUHAL used for input needs to be initialized
  // before anything is done to it.
  OSStatus result = AudioUnitInitialize(input_unit_);
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // We must enable the Audio Unit (AUHAL) for input and disable output
  // BEFORE setting the AUHAL's current device.
  result = EnableIO();
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  result = SetInputDeviceAsCurrent(input_id);
  OSSTATUS_DCHECK(result == noErr, result);

  return result;
}

OSStatus AudioSynchronizedStream::EnableIO() {
  // Enable input on the AUHAL.
  UInt32 enable_io = 1;
  OSStatus result = AudioUnitSetProperty(
      input_unit_,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Input,
      1,  // input element
      &enable_io,
      sizeof(enable_io));

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Disable Output on the AUHAL.
  enable_io = 0;
  result = AudioUnitSetProperty(
      input_unit_,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Output,
      0,  // output element
      &enable_io,
      sizeof(enable_io));

  OSSTATUS_DCHECK(result == noErr, result);
  return result;
}

OSStatus AudioSynchronizedStream::SetupOutput(AudioDeviceID output_id) {
  OSStatus result = noErr;

  result = SetOutputDeviceAsCurrent(output_id);
  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Tell the output unit not to reset timestamps.
  // Otherwise sample rate changes will cause sync loss.
  UInt32 start_at_zero = 0;
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioOutputUnitProperty_StartTimestampsAtZero,
      kAudioUnitScope_Global,
      0,
      &start_at_zero,
      sizeof(start_at_zero));

  OSSTATUS_DCHECK(result == noErr, result);

  return result;
}

OSStatus AudioSynchronizedStream::SetupCallbacks() {
  // Set the input callback.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  OSStatus result = AudioUnitSetProperty(
      input_unit_,
      kAudioOutputUnitProperty_SetInputCallback,
      kAudioUnitScope_Global,
      0,
      &callback,
      sizeof(callback));

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Set the output callback.
  callback.inputProc = OutputProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      output_unit_,
      kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Input,
      0,
      &callback,
      sizeof(callback));

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Set the varispeed callback.
  callback.inputProc = VarispeedProc;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      varispeed_unit_,
      kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Input,
      0,
      &callback,
      sizeof(callback));

  OSSTATUS_DCHECK(result == noErr, result);

  return result;
}

OSStatus AudioSynchronizedStream::SetupStreamFormats() {
  AudioStreamBasicDescription asbd, asbd_dev1_in, asbd_dev2_out;

  // Get the Stream Format (Output client side).
  UInt32 property_size = sizeof(asbd_dev1_in);
  OSStatus result = AudioUnitGetProperty(
      input_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Input,
      1,
      &asbd_dev1_in,
      &property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Get the Stream Format (client side).
  property_size = sizeof(asbd);
  result = AudioUnitGetProperty(
      input_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Output,
      1,
      &asbd,
      &property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Get the Stream Format (Output client side).
  property_size = sizeof(asbd_dev2_out);
  result = AudioUnitGetProperty(
      output_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Output,
      0,
      &asbd_dev2_out,
      &property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Set the format of all the AUs to the input/output devices channel count.
  // For a simple case, you want to set this to
  // the lower of count of the channels in the input device vs output device.
  asbd.mChannelsPerFrame = std::min(asbd_dev1_in.mChannelsPerFrame,
                                    asbd_dev2_out.mChannelsPerFrame);

  // We must get the sample rate of the input device and set it to the
  // stream format of AUHAL.
  Float64 rate = 0;
  property_size = sizeof(rate);

  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyNominalSampleRate;
  pa.mScope = kAudioObjectPropertyScopeWildcard;
  pa.mElement = kAudioObjectPropertyElementMaster;
  result = AudioObjectGetPropertyData(
      input_info_.id_,
      &pa,
      0,
      0,
      &property_size,
      &rate);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  input_sample_rate_ = rate;

  asbd.mSampleRate = rate;
  property_size = sizeof(asbd);

  // Set the new formats to the AUs...
  result = AudioUnitSetProperty(
      input_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Output,
      1,
      &asbd,
      property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  result = AudioUnitSetProperty(
      varispeed_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Input,
      0,
      &asbd,
      property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Set the correct sample rate for the output device,
  // but keep the channel count the same.
  property_size = sizeof(rate);

  pa.mSelector = kAudioDevicePropertyNominalSampleRate;
  pa.mScope = kAudioObjectPropertyScopeWildcard;
  pa.mElement = kAudioObjectPropertyElementMaster;
  result = AudioObjectGetPropertyData(
      output_info_.id_,
      &pa,
      0,
      0,
      &property_size,
      &rate);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  output_sample_rate_ = rate;

  // The requested sample-rate must match the hardware sample-rate.
  if (output_sample_rate_ != params_.sample_rate()) {
    LOG(ERROR) << "Requested sample-rate: " << params_.sample_rate()
        <<  " must match the hardware sample-rate: " << output_sample_rate_;
    return kAudioDeviceUnsupportedFormatError;
  }

  asbd.mSampleRate = rate;
  property_size = sizeof(asbd);

  // Set the new audio stream formats for the rest of the AUs...
  result = AudioUnitSetProperty(
      varispeed_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Output,
      0,
      &asbd,
      property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  result = AudioUnitSetProperty(
      output_unit_,
      kAudioUnitProperty_StreamFormat,
      kAudioUnitScope_Input,
      0,
      &asbd,
      property_size);

  OSSTATUS_DCHECK(result == noErr, result);
  return result;
}

void AudioSynchronizedStream::AllocateInputData() {
  // Allocate storage for the AudioBufferList used for the
  // input data from the input AudioUnit.
  // We allocate enough space for with one AudioBuffer per channel.
  size_t malloc_size = offsetof(AudioBufferList, mBuffers[0]) +
      (sizeof(AudioBuffer) * channels_);

  input_buffer_list_ = static_cast<AudioBufferList*>(malloc(malloc_size));
  input_buffer_list_->mNumberBuffers = channels_;

  input_bus_ = AudioBus::Create(channels_, hardware_buffer_size_);
  wrapper_bus_ = AudioBus::CreateWrapper(channels_);

  // Allocate buffers for AudioBufferList.
  UInt32 buffer_size_bytes = input_bus_->frames() * sizeof(Float32);
  for (size_t i = 0; i < input_buffer_list_->mNumberBuffers; ++i) {
    input_buffer_list_->mBuffers[i].mNumberChannels = 1;
    input_buffer_list_->mBuffers[i].mDataByteSize = buffer_size_bytes;
    input_buffer_list_->mBuffers[i].mData = input_bus_->channel(i);
  }
}

OSStatus AudioSynchronizedStream::HandleInputCallback(
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  TRACE_EVENT0("audio", "AudioSynchronizedStream::HandleInputCallback");

  if (first_input_time_ < 0.0)
    first_input_time_ = time_stamp->mSampleTime;

  // Get the new audio input data.
  OSStatus result = AudioUnitRender(
      input_unit_,
      io_action_flags,
      time_stamp,
      bus_number,
      number_of_frames,
      input_buffer_list_);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Buffer input into FIFO.
  int available_frames = fifo_.max_frames() - fifo_.frames();
  if (input_bus_->frames() <= available_frames)
    fifo_.Push(input_bus_.get());

  return result;
}

OSStatus AudioSynchronizedStream::HandleVarispeedCallback(
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  // Create a wrapper bus on the AudioBufferList.
  WrapBufferList(io_data, wrapper_bus_.get(), number_of_frames);

  if (fifo_.frames() < static_cast<int>(number_of_frames)) {
    // We don't DCHECK here, since this is a possible run-time condition
    // if the machine is bogged down.
    wrapper_bus_->Zero();
    return noErr;
  }

  // Read from the FIFO to feed the varispeed.
  fifo_.Consume(wrapper_bus_.get(), 0, number_of_frames);

  return noErr;
}

OSStatus AudioSynchronizedStream::HandleOutputCallback(
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  if (first_input_time_ < 0.0) {
    // Input callback hasn't run yet -> silence.
    ZeroBufferList(io_data);
    return noErr;
  }

  // Use the varispeed playback rate to offset small discrepancies
  // in hardware clocks, and also any differences in sample-rate
  // between input and output devices.

  // Calculate a varispeed rate scalar factor to compensate for drift between
  // input and output.  We use the actual number of frames still in the FIFO
  // compared with the ideal value of |target_fifo_frames_|.
  int delta = fifo_.frames() - target_fifo_frames_;

  // Average |delta| because it can jitter back/forth quite frequently
  // by +/- the hardware buffer-size *if* the input and output callbacks are
  // happening at almost exactly the same time.  Also, if the input and output
  // sample-rates are different then |delta| will jitter quite a bit due to
  // the rate conversion happening in the varispeed, plus the jittering of
  // the callbacks.  The average value is what's important here.
  average_delta_ += (delta - average_delta_) * 0.1;

  // Compute a rate compensation which always attracts us back to the
  // |target_fifo_frames_| over a period of kCorrectionTimeSeconds.
  const double kCorrectionTimeSeconds = 0.1;
  double correction_time_frames = kCorrectionTimeSeconds * output_sample_rate_;
  fifo_rate_compensation_ =
      (correction_time_frames + average_delta_) / correction_time_frames;

  // Adjust for FIFO drift.
  OSStatus result = AudioUnitSetParameter(
      varispeed_unit_,
      kVarispeedParam_PlaybackRate,
      kAudioUnitScope_Global,
      0,
      fifo_rate_compensation_,
      0);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Render to the output using the varispeed.
  result = AudioUnitRender(
      varispeed_unit_,
      io_action_flags,
      time_stamp,
      0,
      number_of_frames,
      io_data);

  OSSTATUS_DCHECK(result == noErr, result);
  if (result != noErr)
    return result;

  // Create a wrapper bus on the AudioBufferList.
  WrapBufferList(io_data, wrapper_bus_.get(), number_of_frames);

  // Process in-place!
  source_->OnMoreIOData(wrapper_bus_.get(),
                        wrapper_bus_.get(),
                        AudioBuffersState(0, 0));

  return noErr;
}

OSStatus AudioSynchronizedStream::InputProc(
    void* user_data,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  AudioSynchronizedStream* stream =
      static_cast<AudioSynchronizedStream*>(user_data);
  DCHECK(stream);

  return stream->HandleInputCallback(
      io_action_flags,
      time_stamp,
      bus_number,
      number_of_frames,
      io_data);
}

OSStatus AudioSynchronizedStream::VarispeedProc(
    void* user_data,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  AudioSynchronizedStream* stream =
      static_cast<AudioSynchronizedStream*>(user_data);
  DCHECK(stream);

  return stream->HandleVarispeedCallback(
      io_action_flags,
      time_stamp,
      bus_number,
      number_of_frames,
      io_data);
}

OSStatus AudioSynchronizedStream::OutputProc(
    void* user_data,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames,
    AudioBufferList* io_data) {
  AudioSynchronizedStream* stream =
      static_cast<AudioSynchronizedStream*>(user_data);
  DCHECK(stream);

  return stream->HandleOutputCallback(
      io_action_flags,
      time_stamp,
      bus_number,
      number_of_frames,
      io_data);
}

void AudioSynchronizedStream::AudioDeviceInfo::Initialize(
    AudioDeviceID id, bool is_input) {
  id_ = id;
  is_input_ = is_input;
  if (id_ == kAudioDeviceUnknown)
    return;

  UInt32 property_size = sizeof(buffer_size_frames_);

  AudioObjectPropertyAddress pa;
  pa.mSelector = kAudioDevicePropertyBufferFrameSize;
  pa.mScope = kAudioObjectPropertyScopeWildcard;
  pa.mElement = kAudioObjectPropertyElementMaster;
  OSStatus result = AudioObjectGetPropertyData(
      id_,
      &pa,
      0,
      0,
      &property_size,
      &buffer_size_frames_);

  OSSTATUS_DCHECK(result == noErr, result);
}

}  // namespace media
