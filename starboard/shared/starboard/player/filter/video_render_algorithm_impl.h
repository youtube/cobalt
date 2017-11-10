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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_

#include <list>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class VideoRenderAlgorithmImpl : public VideoRenderAlgorithm {
 public:
  VideoRenderAlgorithmImpl();
  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override;
  int GetDroppedFrames() override { return dropped_frames_; }

 private:
  SbMediaTime last_frame_pts_;
  int dropped_frames_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_IMPL_H_
