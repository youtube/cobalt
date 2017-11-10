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

#ifndef STARBOARD_RASPI_SHARED_VIDEO_RENDERER_SINK_IMPL_H_
#define STARBOARD_RASPI_SHARED_VIDEO_RENDERER_SINK_IMPL_H_

#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/types.h"

namespace starboard {
namespace raspi {
namespace shared {

class VideoRendererSinkImpl
    : public ::starboard::shared::starboard::player::filter::VideoRendererSink,
      private ::starboard::shared::starboard::player::JobQueue::JobOwner {
 public:
  typedef ::starboard::shared::starboard::player::JobQueue JobQueue;

  VideoRendererSinkImpl(SbPlayer player, JobQueue* job_queue);
  ~VideoRendererSinkImpl() override;

 private:
  void SetRenderCB(RenderCB render_cb) override;
  void SetBounds(int z_index, int x, int y, int width, int height) override;
  void Update();

  DrawFrameStatus DrawFrame(const scoped_refptr<VideoFrame>& frame,
                            int64_t release_time_in_nanoseconds);

  SbPlayer player_;
  RenderCB render_cb_;

  Mutex mutex_;
  int z_index_;
  int x_;
  int y_;
  int width_;
  int height_;
};

}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_VIDEO_RENDERER_SINK_IMPL_H_
