// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/audio_sink/stub_audio_sink_type.h"

#include <algorithm>

#include "starboard/common/mutex.h"
#include "starboard/common/time.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace audio_sink {
namespace {

class StubAudioSink : public SbAudioSinkPrivate {
 public:
  StubAudioSink(Type* type,
                int sampling_frequency_hz,
                SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                ConsumeFramesFunc consume_frames_func,
                void* context);
  ~StubAudioSink() override;

  bool IsType(Type* type) override { return type_ == type; }
  void SetPlaybackRate(double playback_rate) override { SB_NOTIMPLEMENTED(); }

  void SetVolume(double volume) override {}

 private:
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();

  Type* type_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  ConsumeFramesFunc consume_frames_func_;
  void* context_;

  int sampling_frequency_hz_;

  SbThread audio_out_thread_;
  ::starboard::Mutex mutex_;

  bool destroying_;
};

StubAudioSink::StubAudioSink(
    Type* type,
    int sampling_frequency_hz,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
    void* context)
    : type_(type),
      sampling_frequency_hz_(sampling_frequency_hz),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      context_(context),
      audio_out_thread_(kSbThreadInvalid),
      destroying_(false) {
  audio_out_thread_ =
      SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "stub_audio_out", &StubAudioSink::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(audio_out_thread_));
}

StubAudioSink::~StubAudioSink() {
  {
    ScopedLock lock(mutex_);
    destroying_ = true;
  }
  SbThreadJoin(audio_out_thread_, NULL);
}

// static
void* StubAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  StubAudioSink* sink = reinterpret_cast<StubAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

void StubAudioSink::AudioThreadFunc() {
  const int kMaxFramesToConsumePerRequest = 1024;

  for (;;) {
    {
      ScopedLock lock(mutex_);
      if (destroying_) {
        break;
      }
    }
    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    if (is_playing) {
      int frames_to_consume =
          std::min(kMaxFramesToConsumePerRequest, frames_in_buffer);

      SbThreadSleep(frames_to_consume * 1'000'000LL / sampling_frequency_hz_);
      consume_frames_func_(frames_to_consume, CurrentMonotonicTime(), context_);
    } else {
      // Wait for five millisecond if we are paused.
      SbThreadSleep(5'000);
    }
  }
}

}  // namespace

SbAudioSink StubAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    void* context) {
  return new StubAudioSink(this, sampling_frequency_hz,
                           update_source_status_func, consume_frames_func,
                           context);
}

}  // namespace audio_sink
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
