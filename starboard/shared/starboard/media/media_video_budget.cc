// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_video_budget.h"

#include "build/build_config.h"
#include "starboard/shared/starboard/media/resolutions.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {

namespace {
constexpr int kVideoBufferBudget1080p = 30 * 1024 * 1024;
constexpr int kVideoBufferBudget4KSdr = 100 * 1024 * 1024;
constexpr int kVideoBufferBudget4KHdr = 160 * 1024 * 1024;
}  // namespace

int GetAreaBasedVideoBufferBudget(Size video_size, int bits_per_pixel) {
  if (video_size.IsEmpty() ||
      video_size.GetArea() <= Resolution::k1080p.GetArea()) {
    return kVideoBufferBudget1080p;
  } else if (video_size.GetArea() <= Resolution::k4k.GetArea()) {
    if (bits_per_pixel <= 8) {
      return kVideoBufferBudget4KSdr;
    } else {
      return kVideoBufferBudget4KHdr;
    }
  } else {
    return kVideoBufferBudgetAbove4K;
  }
}

int GetLegacyVideoBufferBudget(Size video_size, int bits_per_pixel) {
  if (video_size.FitsWithin(starboard::Resolution::k1080p) ||
      video_size.IsEmpty()) {
    return kVideoBufferBudget1080p;
  } else if (video_size.FitsWithin(starboard::Resolution::k4k)) {
    if (bits_per_pixel <= 8) {
      return kVideoBufferBudget4KSdr;
    } else {
      return kVideoBufferBudget4KHdr;
    }
  } else {
    return kVideoBufferBudgetAbove4K;
  }
}

int GetDefaultVideoBufferBudget(Size video_size, int bits_per_pixel) {
#if BUILDFLAG(IS_ANDROID)
  if (starboard::features::FeatureList::IsEnabled(
          starboard::features::kAreaBasedVideoBufferBudget)) {
    return GetAreaBasedVideoBufferBudget(video_size, bits_per_pixel);
  } else {
    return GetLegacyVideoBufferBudget(video_size, bits_per_pixel);
  }
#else
  // Non-Android platforms use the new area-based logic directly.
  return GetAreaBasedVideoBufferBudget(video_size, bits_per_pixel);
#endif
}

}  // namespace starboard
