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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_

#include <functional>
#include <list>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_frame_cadence_pattern_generator.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_frame_rate_estimator.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard::shared::starboard::player::filter {

class VideoRenderAlgorithmImpl : public VideoRenderAlgorithm {
 public:
  typedef std::function<double()> GetRefreshRateFn;

  explicit VideoRenderAlgorithmImpl(
      const GetRefreshRateFn& get_refresh_rate_fn = GetRefreshRateFn());

  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override;
  void Seek(int64_t seek_to_time) override;
  int GetDroppedFrames() override { return dropped_frames_; }

 private:
  void RenderWithCadence(MediaTimeProvider* media_time_provider,
                         std::list<scoped_refptr<VideoFrame>>* frames,
                         VideoRendererSink::DrawFrameCB draw_frame_cb);

  const GetRefreshRateFn get_refresh_rate_fn_;

  VideoFrameCadencePatternGenerator cadence_pattern_generator_;
  VideoFrameRateEstimator frame_rate_estimate_;

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  static constexpr int kMaxLogPerPlaybackSession = 32;

  int times_logged_ = 0;
  int64_t media_time_of_last_render_call_;   // microseconds
  int64_t system_time_of_last_render_call_;  // microseconds
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  int64_t last_frame_timestamp_ = -1;  // microseconds
  int current_frame_rendered_times_ = -1;
  int dropped_frames_ = 0;
};

}  // namespace starboard::shared::starboard::player::filter

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_
