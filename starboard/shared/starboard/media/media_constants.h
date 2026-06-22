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

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {
namespace media {

// The following budget variables are mutable (non-const) to allow runtime
// experimentation and overrides (e.g., via JS web player / H5VCC).
extern std::atomic<int> g_video_buffer_budget_1080p;
extern std::atomic<int> g_video_buffer_budget_4k_sdr;
extern std::atomic<int> g_video_buffer_budget_4k_hdr;
extern std::atomic<int> g_video_buffer_budget_above_4k;

constexpr int k1080pArea = 1920 * 1080;
constexpr int k4KArea = 3840 * 2160;

}  // namespace media
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_CONSTANTS_H_
