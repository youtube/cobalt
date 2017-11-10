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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_

#include "starboard/atomic.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class PunchoutVideoRendererSink : public VideoRendererSink {
 public:
  PunchoutVideoRendererSink(SbPlayer player, SbTime render_interval);
  ~PunchoutVideoRendererSink() override;

 private:
  void SetRenderCB(RenderCB render_cb) override;
  void SetBounds(int z_index, int x, int y, int width, int height) override;
  void RunLoop();

  DrawFrameStatus DrawFrame(const scoped_refptr<VideoFrame>& frame,
                            int64_t release_time_in_nanoseconds);

  static void* ThreadEntryPoint(void* context);

  SbPlayer player_;
  SbTime render_interval_;
  RenderCB render_cb_;
  SbThread thread_;
  atomic_bool stop_requested_;

  Mutex mutex_;
  int z_index_;
  int x_;
  int y_;
  int width_;
  int height_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_
