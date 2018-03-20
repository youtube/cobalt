// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/nplb/audio_sink_helpers.h"

namespace starboard {
namespace nplb {

namespace {

SbMediaAudioSampleType GetAnySupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(
          kSbMediaAudioSampleTypeInt16Deprecated)) {
    return kSbMediaAudioSampleTypeInt16Deprecated;
  }
  return kSbMediaAudioSampleTypeFloat32;
}

SbMediaAudioFrameStorageType GetAnySupportedFrameStorageType() {
  if (SbAudioSinkIsAudioFrameStorageTypeSupported(
          kSbMediaAudioFrameStorageTypeInterleaved)) {
    return kSbMediaAudioFrameStorageTypeInterleaved;
  }
  return kSbMediaAudioFrameStorageTypePlanar;
}

}  // namespace

AudioSinkTestFrameBuffers::AudioSinkTestFrameBuffers(int channels)
    : channels_(channels),
      sample_type_(GetAnySupportedSampleType()),
      storage_type_(GetAnySupportedFrameStorageType()) {
  Init();
}

AudioSinkTestFrameBuffers::AudioSinkTestFrameBuffers(
    int channels,
    SbMediaAudioSampleType sample_type)
    : channels_(channels),
      sample_type_(sample_type),
      storage_type_(GetAnySupportedFrameStorageType()) {
  Init();
}

AudioSinkTestFrameBuffers::AudioSinkTestFrameBuffers(
    int channels,
    SbMediaAudioFrameStorageType storage_type)
    : channels_(channels),
      sample_type_(GetAnySupportedSampleType()),
      storage_type_(storage_type) {
  Init();
}

AudioSinkTestFrameBuffers::AudioSinkTestFrameBuffers(
    int channels,
    SbMediaAudioSampleType sample_type,
    SbMediaAudioFrameStorageType storage_type)
    : channels_(channels),
      sample_type_(sample_type),
      storage_type_(storage_type) {
  Init();
}

void AudioSinkTestFrameBuffers::Init() {
  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved) {
    frame_buffer_.resize(bytes_per_frame() * channels_ * kFramesPerChannel);
    frame_buffers_.resize(1);
    frame_buffers_[0] = &frame_buffer_[0];
  } else {
    // We make all planar audio channels share the same frame buffer.
    frame_buffer_.resize(bytes_per_frame() * frames_per_channel());
    frame_buffers_.resize(channels_, &frame_buffer_[0]);
  }
}

AudioSinkTestEnvironment::AudioSinkTestEnvironment(
    const AudioSinkTestFrameBuffers& frame_buffers)
    : frame_buffers_(frame_buffers),
      condition_variable_(mutex_),
      update_source_status_call_count_(0),
      frames_appended_(0),
      frames_consumed_(0),
      is_playing_(true) {
  sink_ = SbAudioSinkCreate(
      frame_buffers_.channels(), sample_rate(), frame_buffers_.sample_type(),
      frame_buffers_.storage_type(), frame_buffers_.frame_buffers(),
      frame_buffers_.frames_per_channel(), UpdateSourceStatusFunc,
      ConsumeFramesFunc, this);
}

AudioSinkTestEnvironment::~AudioSinkTestEnvironment() {
  SbAudioSinkDestroy(sink_);
}

void AudioSinkTestEnvironment::SetIsPlaying(bool is_playing) {
  ScopedLock lock(mutex_);
  is_playing_ = is_playing;
}

void AudioSinkTestEnvironment::AppendFrame(int frames_to_append) {
  ScopedLock lock(mutex_);
  frames_appended_ += frames_to_append;
}

int AudioSinkTestEnvironment::GetFrameBufferFreeSpaceAmount() const {
  ScopedLock lock(mutex_);
  int frames_in_buffer = frames_appended_ - frames_consumed_;
  return frame_buffers_.frames_per_channel() - frames_in_buffer;
}

bool AudioSinkTestEnvironment::WaitUntilUpdateStatusCalled() {
  ScopedLock lock(mutex_);
  int update_source_status_call_count = update_source_status_call_count_;
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  while (update_source_status_call_count == update_source_status_call_count_) {
    SbTime time_elapsed = SbTimeGetMonotonicNow() - start;
    if (time_elapsed >= kTimeToTry) {
      return false;
    }
    SbTime time_to_wait = kTimeToTry - time_elapsed;
    condition_variable_.WaitTimed(time_to_wait);
  }
  return true;
}

bool AudioSinkTestEnvironment::WaitUntilSomeFramesAreConsumed() {
  ScopedLock lock(mutex_);
  int frames_consumed = frames_consumed_;
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  while (frames_consumed == frames_consumed_) {
    SbTime time_elapsed = SbTimeGetMonotonicNow() - start;
    if (time_elapsed >= kTimeToTry) {
      return false;
    }
    SbTime time_to_wait = kTimeToTry - time_elapsed;
    condition_variable_.WaitTimed(time_to_wait);
  }
  return true;
}

bool AudioSinkTestEnvironment::WaitUntilAllFramesAreConsumed() {
  ScopedLock lock(mutex_);
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  while (frames_appended_ != frames_consumed_) {
    SbTime time_elapsed = SbTimeGetMonotonicNow() - start;
    if (time_elapsed >= kTimeToTry) {
      return false;
    }
    SbTime time_to_wait = kTimeToTry - time_elapsed;
    condition_variable_.WaitTimed(time_to_wait);
  }
  return true;
}

void AudioSinkTestEnvironment::OnUpdateSourceStatus(int* frames_in_buffer,
                                                    int* offset_in_frames,
                                                    bool* is_playing,
                                                    bool* is_eos_reached) {
  ScopedLock lock(mutex_);
  *frames_in_buffer = frames_appended_ - frames_consumed_;
  *offset_in_frames = frames_appended_ % frame_buffers_.frames_per_channel();
  *is_playing = is_playing_;
  *is_eos_reached = false;
  ++update_source_status_call_count_;
  condition_variable_.Signal();
}

void AudioSinkTestEnvironment::OnConsumeFrames(int frames_consumed
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                               ,
                                               SbTime frames_consumed_at
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                               ) {
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  SB_DCHECK(frames_consumed_at <= SbTimeGetMonotonicNow());
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  ScopedLock lock(mutex_);
  frames_consumed_ += frames_consumed;
  condition_variable_.Signal();
}

// static
void AudioSinkTestEnvironment::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                      int* offset_in_frames,
                                                      bool* is_playing,
                                                      bool* is_eos_reached,
                                                      void* context) {
  AudioSinkTestEnvironment* environment =
      reinterpret_cast<AudioSinkTestEnvironment*>(context);
  environment->OnUpdateSourceStatus(frames_in_buffer, offset_in_frames,
                                    is_playing, is_eos_reached);
}

// static
void AudioSinkTestEnvironment::ConsumeFramesFunc(int frames_consumed,
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                                 SbTime frames_consumed_at,
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                                 void* context) {
  AudioSinkTestEnvironment* environment =
      reinterpret_cast<AudioSinkTestEnvironment*>(context);
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  environment->OnConsumeFrames(frames_consumed, frames_consumed_at);
#else   // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  environment->OnConsumeFrames(frames_consumed);
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
}

}  // namespace nplb
}  // namespace starboard
