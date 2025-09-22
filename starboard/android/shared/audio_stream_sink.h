// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_STREAM_SINK_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_STREAM_SINK_H_

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "starboard/android/shared/audio_stream.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard::android::shared {

class AudioStreamSink
    : public ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl {
 public:
  AudioStreamSink(Type* type,
                  int channels,
                  int sampling_frequency_hz,
                  SbMediaAudioSampleType sample_type,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  int preferred_buffer_size,
                  SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                  ConsumeFramesFunc consume_frames_func,
                  SbAudioSinkPrivate::ErrorFunc error_func,
                  int64_t start_media_time,
                  bool is_web_audio,
                  void* context);
  ~AudioStreamSink() override;

  bool IsAudioTrackValid() const { return stream_ != nullptr; }
  bool IsType(Type* type) override { return type_ == type; }
  void SetPlaybackRate(double playback_rate) override;

  void SetVolume(double volume) override;
  int GetUnderrunCount();
  int GetStartThresholdInFrames();

 private:
  struct SourceState {
    int available_frames;
    int frame_offset;
    bool is_playing;
    bool is_eos_reached;
  };
  SourceState GetSourceState();

  bool OnReadData(void* buffer, int num_frames_to_read);

  void ReportError(bool capability_changed, const std::string& error_message);
  void ReportPlayedFrames();
  bool ReadMoreFrames(void* data, int num_frames_to_read);

  Type* const type_;

  const int channels_;
  const int sample_rate_;
  const int frame_bytes_;
  uint8_t* frame_buffer_;
  const int frames_per_channel_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const ConsumeFramesFunc consume_frames_func_;
  const SbAudioSinkPrivate::ErrorFunc error_func_;
  const int64_t start_time_;  // microseconds
  void* const context_;

  const std::unique_ptr<AudioStream> stream_;

  int pending_frames_in_platform_buffer_ = 0;
  AudioStream::Timestamp last_timestamp_{0, 0};

  bool data_has_arrived_ = false;
  int initial_silence_frames_ = 0;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_STREAM_SINK_H_
