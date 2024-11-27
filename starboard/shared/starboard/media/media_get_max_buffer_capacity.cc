// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/media.h"

int SbMediaGetMaxBufferCapacity(SbMediaVideoCodec codec,
                                int resolution_width,
                                int resolution_height,
                                int bits_per_pixel) {
  if ((resolution_width <= 1920 && resolution_height <= 1080) ||
      resolution_width == kSbMediaVideoResolutionDimensionInvalid ||
      resolution_height == kSbMediaVideoResolutionDimensionInvalid) {
    // The maximum amount of memory that will be used to store media buffers
    // when video resolution is 1080p. If 0, then memory can grow without bound.
    // This must be larger than sum of 1080p video budget and non-video budget.
    return 50 * 1024 * 1024;
  }

  if (resolution_width <= 3840 && resolution_height <= 2160) {
    if (bits_per_pixel <= 8) {
      // The maximum amount of memory that will be used to store media buffers
      // when video resolution is 4k and bit per pixel is lower than 8. If 0,
      // then memory can grow without bound. This must be larger than sum of 4k
      // video budget and non-video budget.
      return 140 * 1024 * 1024;
    } else {
      // The maximum amount of memory that will be used to store media buffers
      // when video resolution is 4k and bit per pixel is greater than 8. If 0,
      // then memory can grow without bound. This must be larger than sum of 4k
      // video budget and non-video budget.
      return 210 * 1024 * 1024;
    }
  }

  // The maximum amount of memory that will be used to store media buffers when
  // video resolution is 8k. If 0, then memory can grow without bound. This
  // must be larger than sum of 8k video budget and non-video budget.
  return 360 * 1024 * 1024;
}
