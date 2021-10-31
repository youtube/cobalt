// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class VideoRenderer {
 public:
  // All of the functions are called on the PlayerWorker thread unless marked
  // otherwise.
  virtual ~VideoRenderer() {}

  virtual void Initialize(const ErrorCB& error_cb,
                          const PrerolledCB& prerolled_cb,
                          const EndedCB& ended_cb) = 0;
  virtual int GetDroppedFrames() const = 0;

  virtual void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) = 0;
  virtual void WriteEndOfStream() = 0;

  virtual void Seek(SbTime seek_to_time) = 0;

  virtual bool IsEndOfStreamWritten() const = 0;
  virtual bool CanAcceptMoreData() const = 0;

  // Both of the following two functions can be called on any threads.
  virtual void SetBounds(int z_index, int x, int y, int width, int height) = 0;
  virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
