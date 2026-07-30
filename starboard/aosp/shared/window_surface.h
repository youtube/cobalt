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

#ifndef STARBOARD_AOSP_SHARED_WINDOW_SURFACE_H_
#define STARBOARD_AOSP_SHARED_WINDOW_SURFACE_H_

#include <android/native_window.h>

namespace starboard {
namespace android {
namespace shared {

// Holds the reference to |window| that backs the Starboard window on AOSP.
void SetWindowSurface(ANativeWindow* window);

// Returns the currently held window or nullptr if there is no surface.
ANativeWindow* AcquireWindowSurface();

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_WINDOW_SURFACE_H_
