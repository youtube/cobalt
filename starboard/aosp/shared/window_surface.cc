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

#include "starboard/aosp/shared/window_surface.h"

#include <android/native_window.h>

#include <condition_variable>
#include <mutex>

namespace {

std::mutex g_window_mutex;
std::condition_variable g_window_cv;
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
  if (window != nullptr) {
    g_window_cv.notify_all();
  }
}

ANativeWindow* AcquireWindowSurface() {
  std::lock_guard<std::mutex> lock(g_window_mutex);
  // SetWindowSurface(nullptr) runs on the Android UI thread and can drop
  // the last reference, so acquire/set it under a lock
  if (g_native_window != nullptr) {
    ANativeWindow_acquire(g_native_window);
  }
  return g_native_window;
}

void WaitForWindowSurface() {
  std::unique_lock<std::mutex> lock(g_window_mutex);
  g_window_cv.wait(lock, [] { return g_native_window != nullptr; });
}

bool HasWindowSurface() {
  std::lock_guard<std::mutex> lock(g_window_mutex);
  return g_native_window != nullptr;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
