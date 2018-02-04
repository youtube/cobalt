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

#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

using std::placeholders::_1;
using std::placeholders::_2;

PunchoutVideoRendererSink::PunchoutVideoRendererSink(SbPlayer player,
                                                     SbTime render_interval)
    : player_(player),
      render_interval_(render_interval),
      thread_(kSbThreadInvalid),
      z_index_(0),
      x_(0),
      y_(0),
      width_(0),
      height_(0) {
  SB_DCHECK(SbPlayerIsValid(player));
}

PunchoutVideoRendererSink::~PunchoutVideoRendererSink() {
  if (SbThreadIsValid(thread_)) {
    stop_requested_.store(true);
    SbThreadJoin(thread_, NULL);
  }
}

void PunchoutVideoRendererSink::SetRenderCB(RenderCB render_cb) {
  SB_DCHECK(!render_cb_);
  SB_DCHECK(render_cb);

  render_cb_ = render_cb;

  thread_ = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                           "punchoutvidsink",
                           &PunchoutVideoRendererSink::ThreadEntryPoint, this);
}

void PunchoutVideoRendererSink::SetBounds(int z_index,
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

void PunchoutVideoRendererSink::RunLoop() {
  while (!stop_requested_.load()) {
    render_cb_(std::bind(&PunchoutVideoRendererSink::DrawFrame, this, _1, _2));
    SbThreadSleep(render_interval_);
  }
  ScopedLock lock(mutex_);
  shared::starboard::Application::Get()->HandleFrame(
      player_, VideoFrame::CreateEOSFrame(), 0, 0, 0, 0, 0);
}

PunchoutVideoRendererSink::DrawFrameStatus PunchoutVideoRendererSink::DrawFrame(
    const scoped_refptr<VideoFrame>& frame,
    int64_t release_time_in_nanoseconds) {
  SB_DCHECK(release_time_in_nanoseconds == 0);

  ScopedLock lock(mutex_);
  shared::starboard::Application::Get()->HandleFrame(player_, frame, z_index_,
                                                     x_, y_, width_, height_);
  return kNotReleased;
}

// static
void* PunchoutVideoRendererSink::ThreadEntryPoint(void* context) {
  PunchoutVideoRendererSink* this_ptr =
      static_cast<PunchoutVideoRendererSink*>(context);
  this_ptr->RunLoop();
  return NULL;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
