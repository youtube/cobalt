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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_MAX_VIDEO_RESOLUTION_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_MAX_VIDEO_RESOLUTION_H_

#include <string>

namespace starboard {

// Get max_video_resolution setting set via
// SetMaxVideoResolutionForCurrentThread(). Returns empty string if not set.
std::string GetMaxVideoResolutionForCurrentThread();

// Sets the maximum video resolution string for any subsequently created
// SbPlayer on the current calling thread. Pass nullptr or empty string to
// clear.
void SetMaxVideoResolutionForCurrentThread(const char* max_video_resolution);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_MAX_VIDEO_RESOLUTION_H_
