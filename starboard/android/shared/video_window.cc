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

#include "starboard/android/shared/video_window.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <mutex>

#include "cobalt/android/jni_headers/VideoSurfaceView_jni.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/configuration.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

// Global video surface pointer mutex.
SB_ONCE_INITIALIZE_FUNCTION(std::mutex, GetViewSurfaceMutex)
// Global pointer to the single video surface.
jobject g_j_video_surface = NULL;
// Global pointer to the single video window.
ANativeWindow* g_native_video_window = NULL;
// Global video surface pointer holder.
VideoSurfaceHolder* g_video_surface_holder = NULL;
// Global boolean to indicate if we need to reset SurfaceView after playing
// vertical video.
bool g_reset_surface_on_clear_window = false;

}  // namespace

void JNI_VideoSurfaceView_OnVideoSurfaceChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& surface) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder) {
    g_video_surface_holder->OnSurfaceDestroyed();
    g_video_surface_holder = NULL;
  }
  if (g_j_video_surface) {
    env->DeleteGlobalRef(g_j_video_surface);
    g_j_video_surface = NULL;
  }
  if (g_native_video_window) {
    ANativeWindow_release(g_native_video_window);
    g_native_video_window = NULL;
  }
  if (surface) {
    g_j_video_surface = env->NewGlobalRef(surface.obj());
    g_native_video_window = ANativeWindow_fromSurface(env, surface.obj());
  }
}

void JNI_VideoSurfaceView_SetNeedResetSurface(JNIEnv* env) {
  g_reset_surface_on_clear_window = true;
}

// static
bool VideoSurfaceHolder::IsVideoSurfaceAvailable() {
  // We only consider video surface is available when there is a video
  // surface and it is not held by any decoder, i.e.
  // g_video_surface_holder is NULL.
  std::lock_guard lock(*GetViewSurfaceMutex());
  return !g_video_surface_holder && g_j_video_surface;
}

jobject VideoSurfaceHolder::AcquireVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder != NULL) {
    return NULL;
  }
  if (!g_j_video_surface) {
    return NULL;
  }
  g_video_surface_holder = this;
  return g_j_video_surface;
}

void VideoSurfaceHolder::ReleaseVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder == this) {
    g_video_surface_holder = NULL;
  }
}

bool VideoSurfaceHolder::GetVideoWindowSize(int* width, int* height) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_native_video_window == NULL) {
    return false;
  } else {
    *width = ANativeWindow_getWidth(g_native_video_window);
    *height = ANativeWindow_getHeight(g_native_video_window);
    return true;
  }
}

void VideoSurfaceHolder::ClearVideoWindow(bool force_reset_surface) {
  // Lock *GetViewSurfaceMutex() here, to avoid releasing g_native_video_window
  // during painting.
  std::lock_guard lock(*GetViewSurfaceMutex());

  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clear video window when it was null.";
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    SB_LOG(INFO) << "Tried to clear video window when JNIEnv was null.";
    return;
  }

  if (force_reset_surface) {
    StarboardBridge::GetInstance()->ResetVideoSurface(env);
    SB_LOG(INFO) << "Video surface has been reset.";
    return;
  } else if (g_reset_surface_on_clear_window) {
    int width = ANativeWindow_getWidth(g_native_video_window);
    int height = ANativeWindow_getHeight(g_native_video_window);
    if (width <= height) {
      StarboardBridge::GetInstance()->ResetVideoSurface(env);
      return;
    }
  }
}

}  // namespace starboard
