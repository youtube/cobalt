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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_

#include <android/native_window.h>
#include <jni.h>

namespace starboard {
namespace android {
namespace shared {

// Returns the surface which video should be rendered.  This is the surface
// that owns the native window returned by |GetVideoWindow|.
jobject GetVideoSurface();

// Returns the native window into which video should be rendered.
ANativeWindow* GetVideoWindow();

// Clear the video window by painting it Black.  This function is safe to call
// regardless of whether the video window has been initialized or not.
void ClearVideoWindow();

// Wait for all outstanding adjustments of video bounds before returning.
void WaitForVideoBoundsUpdate();

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
