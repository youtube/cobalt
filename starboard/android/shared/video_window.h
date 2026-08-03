// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/decode_target.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

class VideoSurfaceHolder {
 public:
  // Return true only if the video surface is available.
  static bool IsVideoSurfaceAvailable();

  // OnSurfaceDestroyed() will be invoked when surface is destroyed. When this
  // function is called, the decoder no longer owns the surface. Calling
  // AcquireVideoSurface(), ReleaseVideoSurface(), GetVideoWindowSize() or
  // ClearVideoWindow() in this function may cause dead lock.
  virtual void OnSurfaceDestroyed() = 0;

 protected:
  ~VideoSurfaceHolder() {}

  // Returns the surface which video should be rendered. Surface cannot be
  // acquired before last holder release the surface.
  jni_zero::ScopedJavaLocalRef<jobject> AcquireVideoSurface();

  // Release the surface to make the surface available for other holder.
  void ReleaseVideoSurface();

  // Cleans up the video surface. If |force_reset| is enabled, we will destroy
  // the video window. Otherwise, we will clear the video window, posting the
  // task to |gpu_provider|.
  void CleanUpVideoWindow(bool force_reset,
                          SbDecodeTargetGraphicsContextProvider* gpu_provider);
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
