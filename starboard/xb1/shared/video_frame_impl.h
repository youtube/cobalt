// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_XB1_SHARED_VIDEO_FRAME_IMPL_H_
#define STARBOARD_XB1_SHARED_VIDEO_FRAME_IMPL_H_

#include <functional>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"

namespace starboard {
namespace xb1 {
namespace shared {

class VideoFrameImpl
    : public ::starboard::shared::starboard::player::filter::VideoFrame {
 public:
  typedef ::starboard::shared::starboard::player::filter::VideoFrame VideoFrame;

  VideoFrameImpl(SbTime timestamp, std::function<void(VideoFrame*)> release_cb)
      : VideoFrame(timestamp), release_cb_(release_cb) {
    SB_DCHECK(release_cb_);
  }
  ~VideoFrameImpl() override { release_cb_(this); }

 private:
  std::function<void(VideoFrame*)> release_cb_;
};

}  // namespace shared
}  // namespace xb1
}  // namespace starboard

#endif  // STARBOARD_XB1_SHARED_VIDEO_FRAME_IMPL_H_
