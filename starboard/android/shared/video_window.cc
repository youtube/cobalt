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

}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_media_VideoSurfaceView_onVideoSurfaceChanged(
    JNIEnv* env,
    jobject unused_this,
    jobject surface) {
  if (native_video_window_) {
    // TODO: Ensure that the decoder isn't still using the window.
    ANativeWindow_release(native_video_window_);
  }
  if (surface) {
    native_video_window_ = ANativeWindow_fromSurface(env, surface);
  } else {
    native_video_window_ = NULL;
  }
}

ANativeWindow* GetVideoWindow() {
  return native_video_window_;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
