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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_

#include <atomic>
#include <cstdint>
#include <mutex>

#include "starboard/common/thread.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {

class PunchoutVideoRendererSink : public VideoRendererSink, public Thread {
 public:
  PunchoutVideoRendererSink(SbPlayer player, int64_t render_interval);
  ~PunchoutVideoRendererSink() override;

 private:
  void SetRenderCB(RenderCB render_cb) override;
  void SetBounds(int z_index, int x, int y, int width, int height) override;
  void Run() override;

  DrawFrameStatus DrawFrame(const scoped_refptr<VideoFrame>& frame,
                            int64_t release_time_in_nanoseconds);

  SbPlayer player_;
  int64_t render_interval_;  // microseconds
  RenderCB render_cb_;
  std::atomic_bool stop_requested_{false};

  std::mutex mutex_;
  int z_index_;
  int x_;
  int y_;
  int width_;
  int height_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PUNCHOUT_VIDEO_RENDERER_SINK_H_
