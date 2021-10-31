// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

VideoRenderAlgorithmImpl::VideoRenderAlgorithmImpl(
    const GetRefreshRateFn& get_refresh_rate_fn)
    : get_refresh_rate_fn_(get_refresh_rate_fn) {
  if (get_refresh_rate_fn_) {
    SB_LOG(INFO) << "VideoRenderAlgorithmImpl will render with cadence control "
                 << "with display refresh rate set at "
                 << get_refresh_rate_fn_();
  }
}

void VideoRenderAlgorithmImpl::Render(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  // TODO: Enable RenderWithCadence() on all platforms, and replace Render()
  //       with RenderWithCadence().
  if (get_refresh_rate_fn_) {
    return RenderWithCadence(media_time_provider, frames, draw_frame_cb);
  }
  SB_DCHECK(media_time_provider);
  SB_DCHECK(frames);

  if (frames->empty() || frames->front()->is_end_of_stream()) {
    return;
  }

  bool is_audio_playing;
  bool is_audio_eos_played;
  bool is_underflow;
  double playback_rate;
  SbTime media_time = media_time_provider->GetCurrentMediaTime(
      &is_audio_playing, &is_audio_eos_played, &is_underflow, &playback_rate);

  // Video frames are synced to the audio timestamp. However, the audio
  // timestamp is not queried at a consistent interval. For example, if the
  // query intervals are 16ms, 17ms, 16ms, 17ms, etc., then a 60fps video may
  // display even frames twice and drop odd frames.
  //
  // The following diagram illustrates the situation using frames that should
  // last 10 units of time:
  //   frame timestamp:   10          20          30          40          50
  //   sample timestamp:   11        19            31         40           51
  // Using logic which drops frames whose timestamp is less than the sample
  // timestamp:
  // * The frame with timestamp 20 is displayed twice (for sample timestamps
  // 11 and 19).
  // * Then the frame with timestamp 30 is dropped.
  // * Then the frame with timestamp 40 is displayed twice (for sample
  //   timestamps 31 and 40).
  // * Then the frame with timestamp 50 is dropped.
  const SbTime kMediaTimeThreshold = kSbTimeSecond / 250;

  // Favor advancing the frame sooner. This addresses the situation where the
  // audio timestamp query interval is a little shorter than a frame. This
  // favors displaying the next frame over displaying the current frame twice.
  //
  // In the above example, this ensures advancement from frame timestamp 20
  // to frame timestamp 30 when the sample time is 19.
  if (is_audio_playing && frames->size() > 1 &&
      frames->front()->timestamp() == last_frame_timestamp_ &&
      last_frame_timestamp_ - kMediaTimeThreshold < media_time) {
    frames->pop_front();
  }

  // Favor displaying the frame for a little longer. This addresses the
  // situation where the audio timestamp query interval is a little longer
  // than a frame.
  //
  // In the above example, this allows frames with timestamps 30 and 50 to be
  // displayed for sample timestamps 31 and 51, respectively. This may sound
  // like frame 30 is displayed twice (for sample timestamps 19 and 31);
  // however, the "early advance" logic from above would force frame 30 to
  // move onto frame 40 on sample timestamp 31.
  while (frames->size() > 1 && !frames->front()->is_end_of_stream() &&
         frames->front()->timestamp() + kMediaTimeThreshold < media_time) {
    if (frames->front()->timestamp() != last_frame_timestamp_) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      auto now = SbTimeGetMonotonicNow();
      SB_LOG(WARNING)
          << "Dropping frame @ " << frames->front()->timestamp()
          << " microseconds, the elasped media time/system time from"
          << " last Render() call are "
          << media_time - media_time_of_last_render_call_ << "/"
          << now - system_time_of_last_render_call_ << " microseconds, with "
          << frames->size() << " frames in the backlog.";
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      ++dropped_frames_;
    }
    frames->pop_front();
  }

  if (is_audio_eos_played) {
    while (frames->size() > 1) {
      frames->pop_front();
    }
  }

  if (!frames->front()->is_end_of_stream()) {
    last_frame_timestamp_ = frames->front()->timestamp();
    if (draw_frame_cb) {
      auto status = draw_frame_cb(frames->front(), 0);
      if (status == VideoRendererSink::kReleased) {
        frames->pop_front();
      }
    }
  }

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  media_time_of_last_render_call_ = media_time;
  system_time_of_last_render_call_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

void VideoRenderAlgorithmImpl::Seek(SbTime seek_to_time) {
  if (get_refresh_rate_fn_) {
    last_frame_timestamp_ = -1;
    current_frame_rendered_times_ = -1;
    cadence_pattern_generator_.Reset(get_refresh_rate_fn_());
    frame_rate_estimate_.Reset();
  }
}

void VideoRenderAlgorithmImpl::RenderWithCadence(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  SB_DCHECK(media_time_provider);
  SB_DCHECK(frames);
  SB_DCHECK(get_refresh_rate_fn_);

  if (frames->empty() || frames->front()->is_end_of_stream()) {
    // Nothing to render.
    return;
  }

  auto refresh_rate = get_refresh_rate_fn_();
  SB_DCHECK(refresh_rate >= 1);
  if (refresh_rate < 1) {
    refresh_rate = 60;
  }

  bool is_audio_playing;
  bool is_audio_eos_played;
  bool is_underflow;
  double playback_rate;
  SbTime media_time = media_time_provider->GetCurrentMediaTime(
      &is_audio_playing, &is_audio_eos_played, &is_underflow, &playback_rate);

  while (frames->size() > 1 && !frames->front()->is_end_of_stream() &&
         frames->front()->timestamp() < media_time) {
    auto second_iter = frames->begin();
    ++second_iter;

    if ((*second_iter)->is_end_of_stream()) {
      break;
    }

    frame_rate_estimate_.Update(*frames);
    auto frame_rate = frame_rate_estimate_.frame_rate();
    SB_DCHECK(frame_rate != VideoFrameRateEstimator::kInvalidFrameRate);
    cadence_pattern_generator_.UpdateRefreshRateAndMaybeReset(refresh_rate);
    if (playback_rate == 0) {
      playback_rate = 1.0;
    }
    cadence_pattern_generator_.UpdateFrameRate(frame_rate * playback_rate);
    SB_DCHECK(cadence_pattern_generator_.has_cadence());

    auto frame_duration =
        static_cast<SbTime>(kSbTimeSecond / refresh_rate);

    if (current_frame_rendered_times_ >=
        cadence_pattern_generator_.GetNumberOfTimesCurrentFrameDisplays()) {
      if (current_frame_rendered_times_ == 0) {
        ++dropped_frames_;
      }
      frames->pop_front();
      cadence_pattern_generator_.AdvanceToNextFrame();
      current_frame_rendered_times_ = 0;
      if (cadence_pattern_generator_.GetNumberOfTimesCurrentFrameDisplays()
          == 0) {
        continue;
      }
      if (frames->front()->timestamp() <= media_time - frame_duration) {
        continue;
      }
      break;
    }

    if ((*second_iter)->timestamp() > media_time - frame_duration) {
      break;
    }

    if (frames->front()->timestamp() != last_frame_timestamp_) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      ++times_logged_;
      auto now = SbTimeGetMonotonicNow();
      SB_LOG_IF(WARNING, times_logged_ < kMaxLogPerPlaybackSession)
          << "Dropping frame @ " << frames->front()->timestamp()
          << " microseconds, the elasped media time/system time from"
          << " last Render() call are "
          << media_time - media_time_of_last_render_call_ << "/"
          << now - system_time_of_last_render_call_ << " microseconds, with "
          << frames->size() << " frames in the backlog.";
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      ++dropped_frames_;
    } else {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      ++times_logged_;
      auto now = SbTimeGetMonotonicNow();
      SB_LOG_IF(WARNING, times_logged_ < kMaxLogPerPlaybackSession)
          << "Frame @ " << frames->front()->timestamp()
          << " microseconds should be displayed "
          << cadence_pattern_generator_.GetNumberOfTimesCurrentFrameDisplays()
          << " times, but is displayed " << current_frame_rendered_times_
          << " times, the elasped media time/system time from last Render()"
          << " call are " << media_time - media_time_of_last_render_call_ << "/"
          << now - system_time_of_last_render_call_ << " microseconds, the"
          << " video is at " << frame_rate_estimate_.frame_rate() << " fps,"
          << " media time is " << media_time << ", backlog " << frames->size()
          << " frames.";
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
    }
    frames->pop_front();
    cadence_pattern_generator_.AdvanceToNextFrame();
  }

  if (is_audio_eos_played) {
    while (frames->size() > 1) {
      frames->pop_front();
    }
  }

  if (frames->size() == 2) {
    // When there are only two frames and the second one is end of stream, we
    // want to advance to it explicitly if the current media time is later than
    // the timestamp of the first frame, and the first frame has been displayed.
    // This ensures that the video can be properly ended when there is no audio
    // stream, where |is_audio_eos_played| will never be true.
    auto second_iter = frames->begin();
    ++second_iter;

    if ((*second_iter)->is_end_of_stream() &&
        media_time >= frames->front()->timestamp() &&
        current_frame_rendered_times_ > 0) {
      frames->pop_front();
    }
  }

  if (!frames->front()->is_end_of_stream()) {
    if (last_frame_timestamp_ == frames->front()->timestamp()) {
      ++current_frame_rendered_times_;
    } else {
      current_frame_rendered_times_ = 1;
      last_frame_timestamp_ = frames->front()->timestamp();
    }
    if (draw_frame_cb) {
      auto status = draw_frame_cb(frames->front(), 0);
      if (status == VideoRendererSink::kReleased) {
        cadence_pattern_generator_.AdvanceToNextFrame();
        frames->pop_front();
      }
    }
  }

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  media_time_of_last_render_call_ = media_time;
  system_time_of_last_render_call_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
