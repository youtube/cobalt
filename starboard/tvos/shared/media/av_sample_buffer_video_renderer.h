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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_VIDEO_RENDERER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_VIDEO_RENDERER_H_

#import <AVFoundation/AVFoundation.h>

#include <memory>
#include <queue>
#include <string>

#include "base/memory/weak_ptr.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#import "starboard/tvos/shared/media/av_video_sample_buffer_builder.h"
#import "starboard/tvos/shared/media/drm_system_platform.h"
#include "starboard/tvos/shared/observer_registry.h"

@class SBDAVSampleBufferDisplayView;

namespace starboard {

class AVSBVideoRenderer : public VideoRenderer, private JobQueue::JobOwner {
 public:
  typedef AVVideoSampleBufferBuilder::AVSampleBuffer AVSampleBuffer;

  // All of the functions are called on the PlayerWorker thread unless marked
  // otherwise.
  AVSBVideoRenderer(JobQueue* job_queue,
                    const VideoStreamInfo& video_stream_info,
                    SbDrmSystem drm_system);
  ~AVSBVideoRenderer();

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  int GetDroppedFrames() const override {
    SB_DCHECK(BelongsToCurrentThread());
    return total_dropped_frames_;
  }

  void WriteSamples(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;

  void Seek(int64_t seek_to_time) override;

  bool IsEndOfStreamWritten() const override;
  bool CanAcceptMoreData() const override;

  // Both of the following two functions can be called on any threads.
  void SetBounds(int z_index, int x, int y, int width, int height) override;
  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

  AVSampleBufferDisplayLayer* GetDisplayLayer() { return display_layer_; }
  void SetMediaTimeOffset(int64_t media_time_offset);
  bool IsUnderflow();

 private:
  static void DisplayLayerNotificationCallback(CFNotificationCenterRef center,
                                               void* observer,
                                               CFNotificationName name,
                                               const void* object,
                                               CFDictionaryRef user_info);

  AVSBVideoRenderer(const AVSBVideoRenderer&) = delete;
  AVSBVideoRenderer& operator=(const AVSBVideoRenderer&) = delete;

  void ReportError(const std::string& message);
  void UpdatePreferredDisplayCriteria();

  void OnSampleBufferBuilderOutput(
      const scoped_refptr<AVSampleBuffer>& sample_buffer);
  void OnSampleBufferBuilderError(const std::string& message);

  void EnqueueSampleBuffers();
  void DelayedEnqueueSampleBuffers();
  void UpdateCachedFramesWatermark();
  int64_t GetCurrentMediaTime() const;
  size_t GetNumberOfCachedFrames() const;
  void CheckIfStreamEnded();
  void OnDecodeError(NSError* error);
  void OnStatusChanged(NSString* key_path);

  const VideoStreamInfo video_stream_info_;
  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;
  ObserverRegistry::Observer observer_;

  // Objective-C objects.
  SBDAVSampleBufferDisplayView* display_view_ = nullptr;
  AVSampleBufferDisplayLayer* display_layer_ = nullptr;
  NSObject* status_observer_ = nullptr;

  DrmSystemPlatform* drm_system_ = nullptr;
  std::unique_ptr<AVVideoSampleBufferBuilder> sample_buffer_builder_;
  std::queue<scoped_refptr<AVSampleBuffer>> video_sample_buffers_;
  JobQueue::JobToken enqueue_sample_buffers_job_token_;

  int64_t seek_to_time_ = 0;
  int64_t media_time_offset_ = 0;
  int buffers_in_sample_builder_ = 0;
  size_t prerolled_frames_ = 0;
  int64_t pts_of_first_written_buffer_ = 0;
  int64_t pts_of_last_output_buffer_ = 0;
  size_t total_written_frames_ = 0;
  size_t total_dropped_frames_ = 0;
  int frames_per_second_ = 0;
  bool seeking_ = false;
  bool error_occurred_ = false;
  bool is_underflow_ = false;
  bool eos_written_ = false;
  bool is_first_sample_written_ = false;
  bool is_cached_frames_below_low_watermark = false;
  std::atomic_bool is_display_layer_flushing_ = {false};

  base::WeakPtrFactory<AVSBVideoRenderer> weak_ptr_factory_{this};
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_VIDEO_RENDERER_H_
