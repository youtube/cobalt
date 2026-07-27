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

#ifndef STARBOARD_ANDROID_SHARED_WINDOW_SURFACE_H_
#define STARBOARD_ANDROID_SHARED_WINDOW_SURFACE_H_

#include <android/native_window.h>

namespace starboard {
namespace android {
namespace shared {

// Holds the single on-screen ANativeWindow that backs the Starboard window on
// AOSP. The Android app's graphics SurfaceView feeds it via JNI (see
// MainActivity / android_main.cc): |window| comes from
// ANativeWindow_fromSurface on surfaceCreated, and is cleared (nullptr) on
// surfaceDestroyed.
//
// Ownership of |window| transfers to this holder; it releases the previous
// window (if any) on each call. SbWindowCreate() reads it back via
// GetWindowSurface(). Because the app gates nativeStartStarboard() on
// surfaceCreated, the window is already set by the time SbWindowCreate() runs,
// so no waiting is needed here (mirroring the synchronous create on the x11 /
// raspi modular platforms, which always have a display available).
void SetWindowSurface(ANativeWindow* window);
ANativeWindow* GetWindowSurface();

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_WINDOW_SURFACE_H_
