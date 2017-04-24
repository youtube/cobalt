/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_BASE_VIDEO_RESOLUTION_H_
#define MEDIA_BASE_VIDEO_RESOLUTION_H_

#include "base/logging.h"
#include "media/base/media_export.h"
#include "ui/gfx/size.h"

namespace media {

// Enumerates the various representations of the resolution of videos.  Note
// that except |kVideoResolutionInvalid|, all other values are guaranteed to be
// in the same order as its (width, height) pair. Note, unlike the other valid
// resolution levels, |kVideoResolutionHighRes| is not a 16:9 resolution.
enum VideoResolution {
  kVideoResolution1080p,    // 1920 x 1080
  kVideoResolution2k,       // 2560 x 1440
  kVideoResolution4k,       // 3840 x 2160
  kVideoResolution5k,       // 5120 × 2880
  kVideoResolution8k,       // 7680 x 4320
  kVideoResolutionHighRes,  // 8192 x 8192
  kVideoResolutionInvalid
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
  if (width <= 5120 && height <= 2880) {
    return kVideoResolution5k;
  }
  if (width <= 7680 && height <= 4320) {
    return kVideoResolution8k;
  }
  if (width <= 8192 && height <= 8192) {
    return kVideoResolutionHighRes;
  }
  DLOG(FATAL) << "Invalid VideoResolution: width: " << width
              << " height: " << height;
  return kVideoResolutionInvalid;
}

inline VideoResolution GetVideoResolution(const gfx::Size& size) {
  return GetVideoResolution(size.width(), size.height());
}

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_RESOLUTION_H_
