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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/experimental_features.h"
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
  VideoRendererImpl(JobQueue* job_queue,
                    std::unique_ptr<VideoDecoder> decoder,
                    MediaTimeProvider* media_time_provider,
                    std::unique_ptr<VideoRenderAlgorithm> algorithm,
                    scoped_refptr<VideoRendererSink> sink,
                    const ExperimentalFeatures& experimental_features);
  ~VideoRendererImpl() override;

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  int GetDroppedFrames() const override {
    return algorithm_->GetDroppedFrames();
  }

  void WriteSamples(const InputBuffers& input_buffers) override;

  void WriteEndOfStream() override;

  void Seek(int64_t seek_to_time) override;
  void SetPlaybackRate(double playback_rate) override;

  bool IsEndOfStreamWritten() const override {
    return end_of_stream_written_.load();
  }
  bool CanAcceptMoreData() const override;

  // Both of the following two functions can be called on any threads.
  void SetBounds(int z_index, const Rect& rect) override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  typedef std::list<scoped_refptr<VideoFrame>> Frames;

  // Both of the following two functions can be called on any threads.
  void OnDecoderStatus(VideoDecoder::Status status,
                       const scoped_refptr<VideoFrame>& frame);
  void Render(VideoRendererSink::DrawFrameCB draw_frame_cb);
  void OnSeekTimeout();
  // Logs how the preroll timeout ended preroll.  |path| is "timer" (the call
  // made when the timeout expired), "poll" (a later call that the timeout
  // re-posted), "event" (a decoded frame after the timeout expired) or
  // "fallback" (mode 2 gave up waiting for a second frame).  Can be called on
  // any thread.
  void LogPrerollTimeout(const char* path, int decoded_frames);
  void WritePendingInputs();

  MediaTimeProvider* const media_time_provider_;
  const std::unique_ptr<VideoRenderAlgorithm> algorithm_;
  scoped_refptr<VideoRendererSink> sink_;
  std::unique_ptr<VideoDecoder> decoder_;
  const std::optional<int> min_input_buffers_;
  const std::optional<int> min_decoded_frames_;
  const bool enable_video_renderer_vsp_adjustment_;
  const bool enable_trivial_optimizations_;
  // The value of Media.VideoPrerollTimeoutMode.  It decides what happens once
  // the preroll timeout has expired:
  // 0: preroll ends when OnSeekTimeout(), which re-posts itself every 25 ms,
  //    finds a decoded frame.
  // 1: as 0, and preroll also ends as soon as a frame is decoded.
  // 2: as 1, but two decoded frames are required.  If a second frame doesn't
  //    come, preroll ends with one frame at the second poll in a row that
  //    finds exactly one.
  const int preroll_timeout_mode_;
  // The number of decoded frames that ends preroll after the timeout expired.
  const int preroll_timeout_min_frames_;

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
  // Set once OnSeekTimeout() has found too few frames to end preroll, so that
  // OnDecoderStatus() can end it.  Never set when |preroll_timeout_mode_| is 0.
  std::atomic_bool preroll_timeout_expired_{false};
  // Bumped whenever buffered frames are cleared (Seek() or kReleaseAllFrames)
  // so OnSeekTimeout() resets |one_frame_polls_| even if a replacement frame is
  // decoded before the next poll runs.
  std::atomic<uint64_t> frames_generation_{0};
  // The following three are only used on the PlayerWorker thread.  The number
  // of calls to OnSeekTimeout() since the last seek, the number of polls in a
  // row that found exactly one frame from the same |frames_generation_| (mode 2
  // only), and the |frames_generation_| observed by |one_frame_polls_|.
  int seek_timeout_calls_ = 0;
  int one_frame_polls_ = 0;
  uint64_t one_frame_polls_generation_ = 0;
  std::atomic<int32_t> input_buffers_sent_{0};
  int64_t seeking_to_time_ = 0;  // microseconds

  // When experiment |enable_video_renderer_vsp_adjustment| is enabled and
  // playabck rate is not 0.0x or 1.0x. Video renderer vsp adjustment will be
  // enabled. Once it's enabled, it cannot be reset.
  bool is_vsp_adjustment_enabled_ = false;
  InputBuffers pending_input_buffers_;
  bool pending_end_of_stream_ = false;

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

  std::atomic<int64_t> first_input_written_at_{0};  // microseconds
  // When the current seek started, which is when the preroll timer was armed.
  std::atomic<int64_t> seek_started_at_{0};  // microseconds
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_
