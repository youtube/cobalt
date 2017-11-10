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

#include "starboard/raspi/shared/video_renderer_sink_impl.h"

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/time.h"

namespace starboard {
namespace raspi {
namespace shared {

using std::placeholders::_1;
using std::placeholders::_2;

VideoRendererSinkImpl::VideoRendererSinkImpl(SbPlayer player,
                                             JobQueue* job_queue)
    : JobOwner(job_queue),
      player_(player),
      z_index_(0),
      x_(0),
      y_(0),
      width_(0),
      height_(0) {
  SB_DCHECK(SbPlayerIsValid(player));
  SB_DCHECK(job_queue);
}

VideoRendererSinkImpl::~VideoRendererSinkImpl() {
  SB_DCHECK(BelongsToCurrentThread());

  ::starboard::shared::starboard::Application::Get()->HandleFrame(
      player_, VideoFrame::CreateEOSFrame(), 0, 0, 0, 0, 0);
}

void VideoRendererSinkImpl::SetRenderCB(RenderCB render_cb) {
  SB_DCHECK(!render_cb_);
  SB_DCHECK(render_cb);

  render_cb_ = render_cb;

  Schedule(std::bind(&VideoRendererSinkImpl::Update, this));
}

void VideoRendererSinkImpl::SetBounds(int z_index,
                                      int x,
                                      int y,
                                      int width,
                                      int height) {
  ScopedLock lock(mutex_);

  z_index_ = z_index;
  x_ = x;
  y_ = y;
  width_ = width;
  height_ = height;
}

void VideoRendererSinkImpl::Update() {
  const SbTime kUpdateInterval = 5 * kSbTimeMillisecond;
  render_cb_(std::bind(&VideoRendererSinkImpl::DrawFrame, this, _1, _2));
  Schedule(std::bind(&VideoRendererSinkImpl::Update, this), kUpdateInterval);
}

VideoRendererSinkImpl::DrawFrameStatus VideoRendererSinkImpl::DrawFrame(
    const scoped_refptr<VideoFrame>& frame,
    int64_t release_time_in_nanoseconds) {
  SB_DCHECK(release_time_in_nanoseconds == 0);

  ScopedLock lock(mutex_);
  ::starboard::shared::starboard::Application::Get()->HandleFrame(
      player_, frame, z_index_, x_, y_, width_, height_);
  return kNotReleased;
}

}  // namespace shared
}  // namespace raspi
}  // namespace starboard
