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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_SINK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_SINK_H_

#include <functional>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// The class is used to display the video frames.
// It is expected that a pointer of this class is passed to RenderCB and the
// DrawFrameCB will be called for each frame that is going to be rendered.
class VideoRendererSink : public RefCountedThreadSafe<VideoRendererSink> {
 public:
  typedef ::starboard::shared::starboard::player::filter::VideoFrame VideoFrame;

  enum DrawFrameStatus { kNotReleased, kReleased };

  // TODO: Add Stop().

  // This function can only be called inside |RenderCB| to render individual
  // frame.  The user of VideoRendererSink shouldn't store a copy of this
  // function.
  typedef std::function<DrawFrameStatus(const scoped_refptr<VideoFrame>& frame,
                                        int64_t release_time_in_nanoseconds)>
      DrawFrameCB;
  typedef std::function<void(DrawFrameCB)> RenderCB;

  virtual ~VideoRendererSink() {}

  virtual void SetRenderCB(RenderCB render_cb) = 0;
  virtual void SetBounds(int z_index, int x, int y, int width, int height) = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_SINK_H_
