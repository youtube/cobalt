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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_H_

#include <list>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// Used by VideoRenderer to pick the best frame to render according to the
// current media time.
class VideoRenderAlgorithm {
 public:
  typedef ::starboard::shared::starboard::player::filter::MediaTimeProvider
      MediaTimeProvider;
  typedef ::starboard::shared::starboard::player::filter::VideoFrame VideoFrame;
  typedef ::starboard::shared::starboard::player::filter::VideoRendererSink
      VideoRendererSink;

  virtual ~VideoRenderAlgorithm() {}

  // |draw_frame_cb| can be empty.  When it is empty, this function simply runs
  // the frame picking algorithm without calling |draw_frame_cb| to render the
  // frame explicitly.
  virtual void Render(MediaTimeProvider* media_time_provider,
                      std::list<scoped_refptr<VideoFrame>>* frames,
                      VideoRendererSink::DrawFrameCB draw_frame_cb) = 0;
  // Called during seek to reset the internal states of VideoRenderAlgorithm.
  // |seek_to_time| (microseconds) will be set to the seek target.
  virtual void Seek(int64_t seek_to_time) = 0;
  virtual int GetDroppedFrames() = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDER_ALGORITHM_H_
