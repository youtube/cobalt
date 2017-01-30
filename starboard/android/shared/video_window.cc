// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/video_window.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "starboard/configuration.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Global pointer to the single video window.
ANativeWindow* native_video_window_ = NULL;

// Red matching the background of the splash screen.
int32_t kYouTubeRedRGB = 0xE62117;

// Fill a native window with a solid color. Doesn't use any EGL or Skia to avoid
// reconfiguring the window.
void fillWithColor(ANativeWindow* window, int32_t color) {
  if (!window) {
    return;
  }

#ifndef NDEBUG
  // Dim the color by half for debug builds to see the transition.
  color = (color & ~0x01010101) >> 1;
#endif

  int pixels_per_write;
  switch (ANativeWindow_getFormat(window)) {
    case WINDOW_FORMAT_RGBA_8888:
    case WINDOW_FORMAT_RGBX_8888:
      pixels_per_write = 1;
      break;
    case WINDOW_FORMAT_RGB_565:
      color = ((color & 0xF80000) >> 8)
          | ((color & 0x00FC00) >> 5)
          | ((color & 0x0000F8) >> 3);
      color |= color << 16;
      pixels_per_write = 2;
      break;
    default:
      return;
  }

  ANativeWindow_Buffer buffer;
  ARect dirty;
  if (ANativeWindow_lock(window, &buffer, &dirty)) {
    return;
  }

  int32_t* p = static_cast<int32_t*>(buffer.bits);
  int32_t* end = p + buffer.stride * buffer.height / pixels_per_write;
  while (p < end) {
    *p++ = color;
  }

  ANativeWindow_unlockAndPost(window);
}

}  // namespace

extern "C" SB_EXPORT_PLATFORM
void Java_foo_cobalt_VideoSurfaceView_onVideoSurfaceChanged(
    JNIEnv* env, jobject unused_this, jobject surface) {
  if (native_video_window_) {
    // TODO: Ensure that the decoder isn't still using the window.
    ANativeWindow_release(native_video_window_);
  }
  if (surface == NULL) {
    native_video_window_ = NULL;
  } else {
    native_video_window_ = ANativeWindow_fromSurface(env, surface);
  }

  // Fill the window to avoid a "black flash" of seeing the video window
  // through the transparent UI window before the splash screen is drawn.
  fillWithColor(native_video_window_, kYouTubeRedRGB);
}

ANativeWindow* GetVideoWindow() {
  return native_video_window_;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
