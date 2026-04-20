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

int SbMediaGetVideoBufferBudget(SbMediaVideoCodec codec,
                                int resolution_width,
                                int resolution_height,
                                int bits_per_pixel) {
  if ((resolution_width <= 1920 && resolution_height <= 1080) ||
      resolution_width == kSbMediaVideoResolutionDimensionInvalid ||
      resolution_height == kSbMediaVideoResolutionDimensionInvalid) {
    // Specifies the maximum amount of memory used by video buffers of media
    // source before triggering a garbage collection when the video resolution
    // is lower than 1080p (1920x1080).
    return 30 * 1024 * 1024;
  }

  if (resolution_width <= 3840 && resolution_height <= 2160) {
    if (bits_per_pixel <= 8) {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when the video resolution
      // is lower than 4k (3840x2160) and bit per pixel is lower than 8.
      return 100 * 1024 * 1024;
    } else {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when video resolution is
      // lower than 4k (3840x2160) and bit per pixel is greater than 8.
      return 160 * 1024 * 1024;
    }
  }

  // Specifies the maximum amount of memory used by video buffers of media
  // source before triggering a garbage collection when the video resolution is
  // lower than 8k (7680x4320).
  return 300 * 1024 * 1024;
}
