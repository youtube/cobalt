// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_

#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

VideoRenderAlgorithmImpl::VideoRenderAlgorithmImpl()
    : last_frame_pts_(-1), dropped_frames_(0) {}

void VideoRenderAlgorithmImpl::Render(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  SB_DCHECK(media_time_provider);
  SB_DCHECK(frames);
  SB_DCHECK(draw_frame_cb);

  if (frames->empty()) {
    return;
  }
  bool is_audio_playing;
  bool is_audio_eos_played;
  SbMediaTime media_time = media_time_provider->GetCurrentMediaTime(
      &is_audio_playing, &is_audio_eos_played);

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
  const SbMediaTime kMediaTimeThreshold = kSbMediaTimeSecond / 250;

  // Favor advancing the frame sooner. This addresses the situation where the
  // audio timestamp query interval is a little shorter than a frame. This
  // favors displaying the next frame over displaying the current frame twice.
  //
  // In the above example, this ensures advancement from frame timestamp 20
  // to frame timestamp 30 when the sample time is 19.
  if (frames->size() > 1 && frames->front()->pts() == last_frame_pts_ &&
      last_frame_pts_ - kMediaTimeThreshold < media_time) {
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
  while (frames->size() > 1 &&
         frames->front()->pts() + kMediaTimeThreshold < media_time) {
    if (frames->front()->pts() != last_frame_pts_) {
      ++dropped_frames_;
    }
    frames->pop_front();
  }

  if (is_audio_eos_played) {
    while (frames->size() > 1) {
      frames->pop_back();
    }
  }

  if (!frames->front()->is_end_of_stream()) {
    last_frame_pts_ = frames->front()->pts();
    auto status = draw_frame_cb(frames->front(), 0);
    if (status == VideoRendererSink::kReleased) {
      frames->pop_front();
    }
  }
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
