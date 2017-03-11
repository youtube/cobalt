// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_FRAME_RATE_THROTTLER_H_
#define COBALT_RENDERER_FRAME_RATE_THROTTLER_H_

#include "base/time.h"

namespace cobalt {
namespace renderer {

// The FrameRateThrottler is used to enforce a minimum frame time. The
// rasterizer should call the provided hooks just before and after submitting
// a new frame. This can be used to throttle the frame rate (to 30 Hz for
// example) when the presentation interval cannot be specified.
class FrameRateThrottler {
 public:
  // To be called just after submitting a new frame.
  void BeginInterval();

  // To be called just before submitting a new frame.
  void EndInterval();

 private:
  base::TimeTicks begin_time_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_FRAME_RATE_THROTTLER_H_
