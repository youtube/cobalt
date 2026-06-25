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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_VIDEO_BUDGET_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_VIDEO_BUDGET_H_

#include "build/build_config.h"
#include "starboard/common/size.h"

namespace starboard {

// We set the maximum memory budget to 200MB on Android (and 300MB on other
// platforms), balancing the following factors:
//
// 1. 8K Video Playback
// Playing 8K video requires significant memory for decoded frames. For
// instance, a specific device needs to hold up to 6 frames inside its decoder,
// which can take almost 300MB. See b/405467220#comment46 for details. Because
// of this large requirement for decoded frames, we can't allocate too much
// budget for encoded frames.
//
// 2. Chromium's Budget
// Chromium has a max memory budget of 150MB.
// https://github.com/youtube/cobalt/blob/a3c966f929aabea1d71813c31d404e1b319c2fcd/media/base/demuxer_memory_limit.h#L44
#if BUILDFLAG(IS_ANDROID)
constexpr int kVideoBufferBudgetAbove4K = 200 * 1024 * 1024;
#else
constexpr int kVideoBufferBudgetAbove4K = 300 * 1024 * 1024;
#endif  // BUILDFLAG(IS_ANDROID)

// Calculates the video buffer budget based on the resolution area (width *
// height).
int GetAreaBasedVideoBufferBudget(Size resolution, int bits_per_pixel);

// Calculates the video buffer budget based on the legacy logic (width and
// height thresholds).
int GetLegacyVideoBufferBudget(Size resolution, int bits_per_pixel);

// Returns either the area-based or legacy budget based on platform capabilities
// and features (e.g. Android's AreaBasedVideoBufferBudget feature flag).
int GetDefaultVideoBufferBudget(Size resolution, int bits_per_pixel);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_VIDEO_BUDGET_H_
