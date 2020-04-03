/*
 * Copyright 2016 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_MEDIA_BASE_VIDEO_RESOLUTION_H_
#define COBALT_MEDIA_BASE_VIDEO_RESOLUTION_H_

#include "cobalt/math/size.h"
#include "starboard/media.h"

namespace cobalt {
namespace media {

// Enumerates the various representations of the resolution of videos.  Note
// that except |kVideoResolutionInvalid|, all other values are guaranteed to be
// in the same order as its (width, height) pair.
enum VideoResolution {
  kVideoResolution1080p,  // 1920 x 1080
  kVideoResolution2k,     // 2560 x 1440
  kVideoResolution4k,     // 3840 x 2160
  kVideoResolutionInvalid,
};

inline VideoResolution GetVideoResolution(int width, int height) {
  if (width <= 1920 && height <= 1080) {
    return kVideoResolution1080p;
  }
  if (width <= 2560 && height <= 1440) {
    return kVideoResolution2k;
  }
  if (width <= 3840 && height <= 2160) {
    return kVideoResolution4k;
  }
  return kVideoResolutionInvalid;
}

inline VideoResolution GetVideoResolution(const math::Size& size) {
  return GetVideoResolution(size.width(), size.height());
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_VIDEO_RESOLUTION_H_
