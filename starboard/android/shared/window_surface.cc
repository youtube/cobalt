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

#include "starboard/android/shared/window_surface.h"

#include <android/native_window.h>

#include <mutex>

#include "starboard/android/shared/window_internal.h"
#include "starboard/common/log.h"
#include "starboard/window.h"

namespace {

std::mutex g_window_mutex;
ANativeWindow* g_native_window = nullptr;

}  // namespace

namespace starboard {
namespace android {
namespace shared {

void SetWindowSurface(ANativeWindow* window) {
  std::lock_guard<std::mutex> lock(g_window_mutex);
  if (g_native_window) {
    ANativeWindow_release(g_native_window);
  }
  g_native_window = window;
}

ANativeWindow* GetWindowSurface() {
  std::lock_guard<std::mutex> lock(g_window_mutex);
  return g_native_window;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

SbWindow SbWindowCreate(const SbWindowOptions* options) {
  ANativeWindow* native_window = starboard::android::shared::GetWindowSurface();
  if (native_window == nullptr) {
    // The app gates nativeStartStarboard() on surfaceCreated, so the surface
    // should already be available. If it isn't, fail rather than hand back an
    // invalid window silently masked downstream.
    SB_LOG(ERROR) << "SbWindowCreate: no Android surface available.";
    return kSbWindowInvalid;
  }
  SbWindow window = new SbWindowPrivate();
  // The holder owns the ANativeWindow reference for the app's lifetime; the
  // window only borrows it (released on surfaceDestroyed, not in
  // SbWindowDestroy).
  window->native_window = native_window;
  return window;
}

bool SbWindowDestroy(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }
  delete window;
  return true;
}

void* SbWindowGetPlatformHandle(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return nullptr;
  }
  // EGLNativeWindowType and ANativeWindow* are the same on Android, so the
  // Ozone layer can hand this straight to eglCreateWindowSurface().
  return window->native_window;
}

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if (!SbWindowIsValid(window) || window->native_window == nullptr) {
    return false;
  }
  size->width = ANativeWindow_getWidth(window->native_window);
  size->height = ANativeWindow_getHeight(window->native_window);
  // Assume square pixels; the device-resolution-aware ratio (AndroidTV's
  // window_get_size.cc) needs media bring-up that AOSP doesn't have yet.
  size->video_pixel_ratio = 1.0f;
  return true;
}
