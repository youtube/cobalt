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

#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"

#include <unistd.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/thread.h"

namespace starboard {

using std::placeholders::_1;
using std::placeholders::_2;

PunchoutVideoRendererSink::PunchoutVideoRendererSink(SbPlayer player,
                                                     int64_t render_interval)
    : Thread("punchoutvidsink"),
      player_(player),
      render_interval_(render_interval),
      z_index_(0),
      x_(0),
      y_(0),
      width_(0),
      height_(0) {
  SB_DCHECK(SbPlayerIsValid(player));
}

PunchoutVideoRendererSink::~PunchoutVideoRendererSink() {
  if (join_called()) {
    // Thread already joined or never started (if SetRenderCB wasn't called).
    // Actually join_called() returns true if Join() was called.
    // We want to verify if thread is running.
    // starboard::Thread doesn't expose IsRunning().
    // But Join() is safe to call if Start() was called.
    // If Start() wasn't called, Join() might be no-op or valid.
    // Let's assume SetRenderCB was called if we want to join.
    // The previous code checked `thread_ != 0`.
    // starboard::Thread::Join() handles checking if thread was started (impl
    // detail). But we should signal stop.
  }
  stop_requested_.store(true);
  Join();
}

void PunchoutVideoRendererSink::SetRenderCB(RenderCB render_cb) {
  SB_DCHECK(!render_cb_);
  SB_DCHECK(render_cb);

  render_cb_ = render_cb;

  Start();
}

void PunchoutVideoRendererSink::SetBounds(int z_index,
                                          int x,
                                          int y,
                                          int width,
                                          int height) {
  std::lock_guard lock(mutex_);

  z_index_ = z_index;
  x_ = x;
  y_ = y;
  width_ = width;
  height_ = height;
}

void PunchoutVideoRendererSink::Run() {
  while (!stop_requested_.load()) {
    render_cb_(std::bind(&PunchoutVideoRendererSink::DrawFrame, this, _1, _2));
    if (WaitForJoin(render_interval_)) {
      break;
    }
  }
  std::lock_guard lock(mutex_);
  Application::Get()->HandleFrame(player_, VideoFrame::CreateEOSFrame(), 0, 0,
                                  0, 0, 0);
}

PunchoutVideoRendererSink::DrawFrameStatus PunchoutVideoRendererSink::DrawFrame(
    const scoped_refptr<VideoFrame>& frame,
    int64_t release_time_in_nanoseconds) {
  SB_DCHECK_EQ(release_time_in_nanoseconds, 0);

  std::lock_guard lock(mutex_);
  Application::Get()->HandleFrame(player_, frame, z_index_, x_, y_, width_,
                                  height_);
  return kNotReleased;
}

}  // namespace starboard
