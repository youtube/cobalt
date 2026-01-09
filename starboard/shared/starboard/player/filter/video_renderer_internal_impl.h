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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_IMPL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_IMPL_H_

#include <atomic>
#include <list>
#include <memory>
#include <mutex>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

// A class that sits in between the video decoder, the video sink and the
// pipeline to coordinate data transfer between these parties.
class VideoRendererImpl : public VideoRenderer, private JobQueue::JobOwner {
 public:
  // All of the functions are called on the PlayerWorker thread unless marked
  // otherwise.
  VideoRendererImpl(std::unique_ptr<VideoDecoder> decoder,
                    MediaTimeProvider* media_time_provider,
                    std::unique_ptr<VideoRenderAlgorithm> algorithm,
                    scoped_refptr<VideoRendererSink> sink);
  ~VideoRendererImpl() override;

  void Initialize(ErrorCB error_cb,
                  PrerolledCB prerolled_cb,
                  EndedCB ended_cb) override;
  int GetDroppedFrames() const override {
    return algorithm_->GetDroppedFrames();
  }

  void WriteSamples(const InputBuffers& input_buffers) override;

  void WriteEndOfStream() override;

  void Seek(int64_t seek_to_time) override;

  bool IsEndOfStreamWritten() const override {
    return end_of_stream_written_.load();
  }
  bool CanAcceptMoreData() const override;

  // Both of the following two functions can be called on any threads.
  void SetBounds(int z_index, int x, int y, int width, int height) override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  typedef std::list<scoped_refptr<VideoFrame>> Frames;

  // Both of the following two functions can be called on any threads.
  void OnDecoderStatus(VideoDecoder::Status status,
                       const scoped_refptr<VideoFrame>& frame);
  void Render(VideoRendererSink::DrawFrameCB draw_frame_cb);
  void OnSeekTimeout();

  MediaTimeProvider* const media_time_provider_;
  const std::unique_ptr<VideoRenderAlgorithm> algorithm_;
  scoped_refptr<VideoRendererSink> sink_;
  std::unique_ptr<VideoDecoder> decoder_;

  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  // Our owner will attempt to seek to time 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool first_input_written_ = false;
  std::atomic_bool end_of_stream_written_{false};
  std::atomic_bool end_of_stream_decoded_{false};
  std::atomic_bool ended_cb_called_{false};

  std::atomic_bool need_more_input_{true};
  std::atomic_bool seeking_{false};
  int64_t seeking_to_time_ = 0;  // microseconds

  // |number_of_frames_| = decoder_frames_.size() + sink_frames_.size()
  std::atomic<int32_t> number_of_frames_{0};
  // |sink_frames_| is locked inside VideoRenderer::Render() when calling
  // algorithm_->Render().  So OnDecoderStatus() won't try to lock and append
  // the decoded frames to |sink_frames_| directly to avoid being blocked.  It
  // will append newly decoded frames to |decoder_frames_| instead.  Note that
  // both |decoder_frames_| and |sink_frames_| can be used on multiple threads.
  // When they are being modified at the same time, |decoder_frames_mutex_|
  // should always be locked before |sink_frames_mutex_| to avoid deadlock.
  std::mutex decoder_frames_mutex_;
  Frames decoder_frames_;
  std::mutex sink_frames_mutex_;
  Frames sink_frames_;

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  enum BufferingState {
    kWaitForBuffer,
    kWaitForConsumption,
  };

  static const int64_t kCheckBufferingStateInterval = 1'000'000;  // 1 second
  static const int64_t kDelayBeforeWarning = 2'000'000;           // 2 seconds
  static const int64_t kMinLagWarningInterval = 10'000'000;       // 10 seconds

  static const int64_t kMaxRenderIntervalBeforeWarning = 66'000;     // 66ms
  static const int64_t kMaxGetCurrentDecodeTargetDuration = 16'000;  // 16ms

  void CheckBufferingState();
  void CheckForFrameLag(int64_t last_decoded_frame_timestamp);

  volatile BufferingState buffering_state_ = kWaitForBuffer;
  volatile int64_t last_buffering_state_update_ = 0;       // microseconds
  volatile int64_t last_output_ = 0;                       // microseconds
  mutable volatile int64_t last_can_accept_more_data = 0;  // microseconds

  int64_t time_of_last_lag_warning_;  // microseconds

  int64_t time_of_last_render_call_ = -1;  // microseconds

  int64_t first_input_written_at_ = 0;  // microseconds
#endif                                  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_IMPL_H_
