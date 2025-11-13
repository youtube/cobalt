// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_AUDIO_RENDERER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_AUDIO_RENDERER_H_

#import <AVFoundation/AVFoundation.h>

#include <memory>
#include <string>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#import "starboard/tvos/shared/media/av_audio_sample_buffer_builder.h"
#import "starboard/tvos/shared/media/drm_system_platform.h"
#include "starboard/tvos/shared/observer_registry.h"

@class AVSBAudioRendererStatusObserver;

namespace starboard {

class AVSBAudioRenderer : public AudioRenderer, private JobQueue::JobOwner {
 public:
  AVSBAudioRenderer(const AudioStreamInfo& audio_stream_info,
                    SbDrmSystem drm_system);

  ~AVSBAudioRenderer() override;

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  void WriteSamples(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;

  void SetVolume(double volume) override;

  // TODO: Remove the eos state querying functions and their tests.
  bool IsEndOfStreamWritten() const override;
  bool IsEndOfStreamPlayed() const override;
  bool CanAcceptMoreData() const override;

  AVSampleBufferAudioRenderer* GetAudioRenderer() { return audio_renderer_; }
  void Seek(int64_t seek_to_time, int64_t media_time_offset);
  bool IsUnderflow();

 private:
  AVSBAudioRenderer(const AVSBAudioRenderer&) = delete;
  AVSBAudioRenderer& operator=(const AVSBAudioRenderer&) = delete;

  void ReportError(const std::string& message);

  void UpdatePlayState();
  int64_t GetCurrentMediaTime() const;
  void CheckIfStreamEnded();
  void OnStatusChanged(NSString* key_path);

  AudioStreamInfo audio_stream_info_;
  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;
  ObserverRegistry::Observer observer_;

  // Objective-C objects.
  AVSampleBufferAudioRenderer* audio_renderer_ = nullptr;
  NSObject* status_observer_ = nullptr;

  DrmSystemPlatform* drm_system_ = nullptr;
  std::unique_ptr<AVAudioSampleBufferBuilder> sample_buffer_builder_;

  int64_t seek_to_time_ = 0;
  int64_t media_time_offset_ = 0;
  int prerolled_frames_ = 0;
  int64_t last_buffer_end_time_ = 0;
  bool seeking_ = false;
  bool eos_written_ = false;
  std::atomic_bool eos_played_ = {false};
  bool error_occurred_ = false;
  bool is_underflow_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_AUDIO_RENDERER_H_
