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

#include <list>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
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
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A class that sits in between the video decoder, the video sink and the
// pipeline to coordinate data transfer between these parties.
class VideoRendererImpl : public VideoRenderer, private JobQueue::JobOwner {
 public:
  // All of the functions are called on the PlayerWorker thread unless marked
  // otherwise.
  VideoRendererImpl(scoped_ptr<VideoDecoder> decoder,
                    MediaTimeProvider* media_time_provider,
                    scoped_ptr<VideoRenderAlgorithm> algorithm,
                    scoped_refptr<VideoRendererSink> sink);
  ~VideoRendererImpl() override;

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  int GetDroppedFrames() const override {
    return algorithm_->GetDroppedFrames();
  }

  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;

  void Seek(SbTime seek_to_time) override;

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
  scoped_ptr<VideoRenderAlgorithm> algorithm_;
  scoped_refptr<VideoRendererSink> sink_;
  scoped_ptr<VideoDecoder> decoder_;

  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  SbTimeMonotonic absolute_time_of_first_input_ = 0;
  // Our owner will attempt to seek to time 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool first_input_written_ = false;
  atomic_bool end_of_stream_written_;
  atomic_bool end_of_stream_decoded_;
  atomic_bool ended_cb_called_;

  atomic_bool need_more_input_;
  atomic_bool seeking_;
  SbTime seeking_to_time_ = 0;

  // |number_of_frames_| = decoder_frames_.size() + sink_frames_.size()
  atomic_int32_t number_of_frames_;
  // |sink_frames_| is locked inside VideoRenderer::Render() when calling
  // algorithm_->Render().  So OnDecoderStatus() won't try to lock and append
  // the decoded frames to |sink_frames_| directly to avoid being blocked.  It
  // will append newly decoded frames to |decoder_frames_| instead.  Note that
  // both |decoder_frames_| and |sink_frames_| can be used on multiple threads.
  // When they are being modified at the same time, |decoder_frames_mutex_|
  // should always be locked before |sink_frames_mutex_| to avoid deadlock.
  Mutex decoder_frames_mutex_;
  Frames decoder_frames_;
  Mutex sink_frames_mutex_;
  Frames sink_frames_;

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  enum BufferingState {
    kWaitForBuffer,
    kWaitForConsumption,
  };

  static const SbTimeMonotonic kCheckBufferingStateInterval = kSbTimeSecond;
  static const SbTimeMonotonic kDelayBeforeWarning = 2 * kSbTimeSecond;
  static const SbTimeMonotonic kMinLagWarningInterval = 10 * kSbTimeSecond;

  static const SbTimeMonotonic kMaxRenderIntervalBeforeWarning =
      66 * kSbTimeMillisecond;
  static const SbTimeMonotonic kMaxGetCurrentDecodeTargetDuration =
      16 * kSbTimeMillisecond;

  void CheckBufferingState();
  void CheckForFrameLag(SbTime last_decoded_frame_timestamp);

  volatile BufferingState buffering_state_ = kWaitForBuffer;
  volatile SbTimeMonotonic last_buffering_state_update_ = 0;
  volatile SbTimeMonotonic last_output_ = 0;
  mutable volatile SbTime last_can_accept_more_data = 0;

  SbTimeMonotonic time_of_last_lag_warning_;

  SbTimeMonotonic time_of_last_render_call_ = -1;

  SbTimeMonotonic first_input_written_at_ = 0;
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_IMPL_H_
