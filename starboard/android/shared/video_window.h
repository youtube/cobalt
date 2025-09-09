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

struct ClearNativeWindowGraphicsContextProvider;

typedef void (*ClearNativeWindowGlesContextRunnerTarget)(
    void* gles_context_runner_target_context);

typedef void (*ClearNativeWindowGlesContextRunner)(
    ClearNativeWindowGraphicsContextProvider* provider,
    ClearNativeWindowGlesContextRunnerTarget target_function,
    void* target_function_context);

typedef struct ClearNativeWindowGraphicsContextProvider {
  ClearNativeWindowGlesContextRunner gles_context_runner;
  void* gles_context_runner_context;
} ClearNativeWindowGraphicsContextProvider;

namespace starboard::android::shared {

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
  jobject AcquireVideoSurface();

  // Release the surface to make the surface available for other holder.
  void ReleaseVideoSurface();

  // Get the native window size. Return false if don't have available native
  // window.
  bool GetVideoWindowSize(int* width, int* height);

  // Clear the video window by painting it Black.
  void ClearVideoWindow(bool force_reset_surface);

  void SetGpuProvider(SbDecodeTargetGraphicsContextProvider* gpu_provider);

  // void ClearNativeWindow(ANativeWindow* native_window);

  // GraphicsContextRunner
  // ClearNativeWindowGlesContextRunner gles_context_runner_;

  // Should probably include the egl filese in here
  // starboardrendererwrapper
  // void* gles_context_runner_context_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
