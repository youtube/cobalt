// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "starboard/android/shared/aaudio_loader.h"
#include "starboard/android/shared/audio_track_audio_sink_type.h"  // for Callbacks definition
#include "starboard/audio_sink.h"
#include "starboard/common/pass_key.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace android {

class AAudioAudioSink : public SbAudioSinkImpl {
 public:
  static std::unique_ptr<AAudioAudioSink> Create(
      SbAudioSinkPrivate::Type* type,
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      AudioTrackAudioSinkType::Callbacks callbacks,
      void* context);

  AAudioAudioSink(PassKey<AAudioAudioSink>,
                  SbAudioSinkPrivate::Type* type,
                  int channels,
                  int sampling_frequency_hz,
                  SbMediaAudioSampleType sample_type,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  AudioTrackAudioSinkType::Callbacks callbacks,
                  AAudioStreamBuilder* builder,
                  void* context);
  ~AAudioAudioSink() override;

  bool IsType(SbAudioSinkPrivate::Type* type) override { return type_ == type; }
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  AAudioStream* stream() const { return stream_; }

 private:
  static aaudio_data_callback_result_t AAudioDataCallback(AAudioStream* stream,
                                                          void* userData,
                                                          void* audioData,
                                                          int32_t numFrames);
  static AAudioStream* OpenStreamHelper(AAudioStreamBuilder* builder,
                                        AAudioAudioSink* sink);

  aaudio_data_callback_result_t OnAudioCallback(void* audioData,
                                                int32_t numFrames);
  void PollProgress();
  void TrackAndConsumePlayhead();

  void ApplyVolume(void* buffer, int frames, double volume);

  void ReportError(const std::string& error_message);
  int64_t GetFramesDurationUs(int frames) const;

  const raw_ptr<SbAudioSinkPrivate::Type> type_;
  const int channels_;
  const int sampling_frequency_hz_;
  const SbMediaAudioSampleType sample_type_;
  const raw_ptr<void> frame_buffer_;
  const int frames_per_channel_;
  const AudioTrackAudioSinkType::Callbacks callbacks_;
  const raw_ptr<void> context_;

  const raw_ptr<AAudioLoader> aaudio_;
  const raw_ptr<AAudioStream> stream_;

  const std::unique_ptr<JobThread> job_thread_;
  JobQueue::JobToken progress_job_token_ = JobQueue::JobToken::kUnscheduled;

  std::mutex mutex_;
  double volume_ = 1.0;
  double playback_rate_ = 1.0;
  int64_t last_playback_head_position_ = 0;
};

}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_
