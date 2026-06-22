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

#include "starboard/common/size.h"

namespace starboard {
namespace media {

// Calculates the video buffer budget based on the resolution area (width *
// height).
int GetAreaBasedVideoBufferBudget(Size resolution, int bits_per_pixel);

// Calculates the video buffer budget based on the legacy logic (width and
// height thresholds).
int GetLegacyVideoBufferBudget(Size resolution, int bits_per_pixel);

// Returns either the area-based or legacy budget based on platform capabilities
// and features (e.g. Android's AreaBasedVideoBufferBudget feature flag).
int GetDefaultVideoBufferBudget(Size resolution, int bits_per_pixel);

}  // namespace media
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_VIDEO_BUDGET_H_
