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

#include "starboard/log.h"

#if SB_API_VERSION >= SB_MEDIA_BUFFER_SETTINGS_QUERIES_API_VERSION
int SbMediaGetVideoBufferBudget(SbMediaVideoCodec codec,
                                int resolution_width,
                                int resolution_height,
                                int bits_per_pixel) {
  SB_UNREFERENCED_PARAMETER(codec);
  SB_UNREFERENCED_PARAMETER(bits_per_pixel);
  if ((resolution_width <= 1920 && resolution_height <= 1080) ||
      resolution_width == kSbMediaVideoResolutionDimensionInvalid ||
      resolution_height == kSbMediaVideoResolutionDimensionInvalid) {
#if defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P)
    SB_DLOG(WARNING) << "COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P will be "
                        "deprecated in a future Starboard version.";
    // Use define forwarded from GYP variable.
    return COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P;
#else  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P)
    return 16 * 1024 * 1024;
#endif  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P)
  }
#if defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K)
  SB_DLOG(WARNING) << "COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K will be "
                      "deprecated in a future Starboard version.";
  // Use define forwarded from GYP variable.
  return COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K;
#else  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K)
  return 60 * 1024 * 1024;
#endif  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K)
}
#endif  // SB_API_VERSION >= SB_MEDIA_BUFFER_SETTINGS_QUERIES_API_VERSION
