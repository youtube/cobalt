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

// Holds the single on-screen ANativeWindow that backs the Starboard window on
// AOSP.

// Takes ownership of the caller's reference to |window| and releases the
// reference to any previously held window. Pass nullptr when the Android
// surface is destroyed.
void SetWindowSurface(ANativeWindow* window);

// Returns the currently held window with a reference added for the caller, or
// nullptr if there is no surface. The caller owns that reference and must
// release it with ANativeWindow_release().
ANativeWindow* AcquireWindowSurface();

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_WINDOW_SURFACE_H_
