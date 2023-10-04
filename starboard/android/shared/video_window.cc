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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/once.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Global video surface pointer mutex.
SB_ONCE_INITIALIZE_FUNCTION(Mutex, GetViewSurfaceMutex);
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

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChangedLocked(
    JNIEnv* env,
    jobject unused_this,
    jobject surface) {
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
    g_j_video_surface = env->NewGlobalRef(surface);
    g_native_video_window = ANativeWindow_fromSurface(env, surface);
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChanged(
    JNIEnv* env,
    jobject j_this,
    jobject surface) {
  ScopedLock lock(*GetViewSurfaceMutex());
  Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChangedLocked(
      env, j_this, surface);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeSetNeedResetSurface(
    JNIEnv* env,
    jobject unused_this) {
  g_reset_surface_on_clear_window = true;
}

// static
bool VideoSurfaceHolder::IsVideoSurfaceAvailable() {
  // We only consider video surface is available when there is a video
  // surface and it is not held by any decoder, i.e.
  // g_video_surface_holder is NULL.
  ScopedLock lock(*GetViewSurfaceMutex());
  return !g_video_surface_holder && g_j_video_surface;
}

jobject VideoSurfaceHolder::AcquireVideoSurface() {
  ScopedLock lock(*GetViewSurfaceMutex());
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
  ScopedLock lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder == this) {
    g_video_surface_holder = NULL;
  }
}

bool VideoSurfaceHolder::GetVideoWindowSize(int* width, int* height) {
  ScopedLock lock(*GetViewSurfaceMutex());
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
  ScopedLock lock(*GetViewSurfaceMutex());

  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clear video window when it was null.";
    return;
  }

  JniEnvExt* env = JniEnvExt::Get();
  if (!env) {
    SB_LOG(INFO) << "Tried to clear video window when JniEnvExt was null.";
    return;
  }

  if (force_reset_surface) {
    env->CallStarboardVoidMethodOrAbort("resetVideoSurface", "()V");
    return;
  } else if (g_reset_surface_on_clear_window) {
    int width = ANativeWindow_getWidth(g_native_video_window);
    int height = ANativeWindow_getHeight(g_native_video_window);
    if (width <= height) {
      env->CallStarboardVoidMethodOrAbort("resetVideoSurface", "()V");
      return;
    }
  }
  env->CallStarboardVoidMethodOrAbort("clearVideoSurface", "()V");
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
