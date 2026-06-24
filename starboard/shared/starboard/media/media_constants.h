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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_CONSTANTS_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_CONSTANTS_H_

#include <atomic>

#include "build/build_config.h"
#include "starboard/media.h"

namespace starboard {

constexpr int g_video_buffer_budget_1080p = 30 * 1024 * 1024;
constexpr int g_video_buffer_budget_4k_sdr = 100 * 1024 * 1024;
constexpr int g_video_buffer_budget_4k_hdr = 160 * 1024 * 1024;

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
constexpr int g_video_buffer_budget_above_4k = 200 * 1024 * 1024;
#else
constexpr int g_video_buffer_budget_above_4k = 300 * 1024 * 1024;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_CONSTANTS_H_
