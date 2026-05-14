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
#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/shared/gles/gl_call.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;

namespace {

// Global video surface pointer mutex.
SB_ONCE_INITIALIZE_FUNCTION(std::mutex, GetViewSurfaceMutex)
// Global pointer to the single video surface.
jobject g_j_video_surface = NULL;
// Global pointer to the single video window.
ANativeWindow* g_native_video_window = NULL;

}  // namespace

namespace {

// Global video surface pointer holder.
scoped_refptr<SurfaceDestroyNotifier> g_surface_destroy_notifier = nullptr;

// Global boolean to indicate if we need to reset SurfaceView after playing
// vertical video.
bool g_reset_surface_on_clear_window = false;

}  // namespace

void JNI_VideoSurfaceView_OnVideoSurfaceChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& surface) {
  scoped_refptr<SurfaceDestroyNotifier> notifier_to_notify;
  {
    std::lock_guard lock(*GetViewSurfaceMutex());
    if (g_surface_destroy_notifier) {
      notifier_to_notify = g_surface_destroy_notifier;
      g_surface_destroy_notifier = nullptr;
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

  if (notifier_to_notify) {
    notifier_to_notify->Notify();
  }
}

void JNI_VideoSurfaceView_SetNeedResetSurface(JNIEnv* env) {
  g_reset_surface_on_clear_window = true;
}

void SurfaceDestroyNotifier::RunTask() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!disconnected_ && holder_) {
    holder_->OnSurfaceDestroyed();
  }
  done_ = true;
  cv_.notify_one();
}

// static
bool VideoSurfaceHolder::IsVideoSurfaceAvailable() {
  // We only consider video surface is available when there is a video
  // surface and it is not held by any decoder, i.e.
  // g_surface_destroy_notifier is NULL.
  std::lock_guard lock(*GetViewSurfaceMutex());
  return !g_surface_destroy_notifier && g_j_video_surface;
}

scoped_refptr<SurfaceDestroyNotifier> VideoSurfaceHolder::AcquireVideoSurface(
    JobQueue* job_queue,
    jobject* out_surface) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_surface_destroy_notifier != nullptr) {
    return nullptr;
  }
  if (!g_j_video_surface) {
    return nullptr;
  }
  g_surface_destroy_notifier = new SurfaceDestroyNotifier(this, job_queue);
  *out_surface = g_j_video_surface;
  return g_surface_destroy_notifier;
}

void VideoSurfaceHolder::ReleaseVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_surface_destroy_notifier && g_surface_destroy_notifier->Holds(this)) {
    g_surface_destroy_notifier->Disconnect();
    g_surface_destroy_notifier = nullptr;
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
