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
#include <vector>

#include "cobalt/android/jni_headers/VideoSurfaceView_jni.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"
#include "starboard/common/once.h"
#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/shared/gles/gl_call.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;

namespace {

// Global video surface pointer mutex.
SB_ONCE_INITIALIZE_FUNCTION(std::mutex, GetViewSurfaceMutex)

jni_zero::ScopedJavaGlobalRef<jobject>& GetGlobalVideoSurface() {
  static NoDestructor<jni_zero::ScopedJavaGlobalRef<jobject>> instance;
  return *instance;
}

// Global pointer to the single video window.
ANativeWindow* g_native_video_window = nullptr;

// Global ref-counted video surface destroy notifier wrapped in NoDestructor
// to avoid exit-time destructor issues.
scoped_refptr<SurfaceDestroyNotifier>& GetGlobalSurfaceDestroyNotifier() {
  static NoDestructor<scoped_refptr<SurfaceDestroyNotifier>> notifier;
  return *notifier;
}

void ClearNativeWindow(void* raw_context) {
  ANativeWindow* native_window = static_cast<ANativeWindow*>(raw_context);
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    SB_LOG(ERROR) << "Found no EGL display in ClearNativeWindow";
    return;
  }

  const EGLint kAttributeList[] = {
      EGL_RED_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_ALPHA_SIZE,
      8,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_NONE,
  };
  // First, query how many configs match the given attribute list.
  EGLint num_configs = 0;
  EGL_CALL(eglChooseConfig(display, kAttributeList, NULL, 0, &num_configs));
  if (num_configs == 0) {
    SB_LOG(ERROR) << "No matching EGL configs found in ClearNativeWindow";
    return;
  }

  // Allocate space to receive the matching configs and retrieve them.
  std::vector<EGLConfig> configs(num_configs);
  EGL_CALL(eglChooseConfig(display, kAttributeList, configs.data(), num_configs,
                           &num_configs));
  EGLNativeWindowType egl_native_window =
      static_cast<EGLNativeWindowType>(native_window);
  EGLConfig config;

  // Find the first config that successfully allow a window surface to be
  // created.
  EGLSurface surface = EGL_NO_SURFACE;
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface = eglCreateWindowSurface(display, config, egl_native_window, NULL);
    if (surface != EGL_NO_SURFACE) {
      break;
    }
  }
  if (surface == EGL_NO_SURFACE) {
    SB_LOG(ERROR) << "Found no EGL surface in ClearNativeWindow";
    return;
  }

  // Create an OpenGL ES 2.0 context.
  EGLContext context = EGL_NO_CONTEXT;
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION,
      2,
      EGL_NONE,
  };
  context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib_list);
  if (context == EGL_NO_CONTEXT) {
    SB_LOG(ERROR) << "Failed to create EGL context in ClearNativeWindow";
    EGL_CALL(eglDestroySurface(display, surface));
    return;
  }

  /* connect the context to the surface */
  EGL_CALL(eglMakeCurrent(display, surface, surface, context));

  GL_CALL(glClearColor(0, 0, 0, 1));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
  GL_CALL(glFlush());

  EGL_CALL(eglSwapBuffers(display, surface));

  // Cleanup all used resources.
  EGL_CALL(
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EGL_CALL(eglDestroyContext(display, context));
  EGL_CALL(eglDestroySurface(display, surface));
}

}  // namespace

void JNI_VideoSurfaceView_OnVideoSurfaceChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& surface) {
  SB_LOG(INFO) << "KJ: JNI_VideoSurfaceView_OnVideoSurfaceChanged -> surface="
               << (surface ? "not_null" : "null")
               << ", current g_native_video_window="
               << (g_native_video_window ? "not_null" : "null");
  scoped_refptr<SurfaceDestroyNotifier> notifier_to_notify;
  {
    std::lock_guard lock(*GetViewSurfaceMutex());
    if (GetGlobalSurfaceDestroyNotifier()) {
      notifier_to_notify = GetGlobalSurfaceDestroyNotifier();
      GetGlobalSurfaceDestroyNotifier() = nullptr;
    }
    GetGlobalVideoSurface().Reset();
    if (g_native_video_window) {
      SB_LOG(INFO) << "KJ: JNI_VideoSurfaceView_OnVideoSurfaceChanged -> "
                      "calling ANativeWindow_release(g_native_video_window)";
      ANativeWindow_release(g_native_video_window);
      g_native_video_window = nullptr;
    }
    if (surface) {
      GetGlobalVideoSurface().Reset(env, surface);
      g_native_video_window = ANativeWindow_fromSurface(env, surface.obj());
      SB_LOG(INFO) << "KJ: JNI_VideoSurfaceView_OnVideoSurfaceChanged -> "
                      "acquired new g_native_video_window from surface";
    }
  }

  if (notifier_to_notify) {
    notifier_to_notify->Notify();
  }
}

void SurfaceDestroyNotifier::Disconnect() {
  {
    std::lock_guard lock(mutex_);
    disconnected_ = true;
    job_queue_ = nullptr;
    holder_ = nullptr;
    done_ = true;  // Mark as done_ so Notify() can exit immediately
  }
  done_cv_.notify_one();
}

void SurfaceDestroyNotifier::Notify() {
  std::unique_lock lock(mutex_);
  if (disconnected_ || !holder_ || !job_queue_) {
    return;
  }

  done_ = false;
  scoped_refptr<SurfaceDestroyNotifier> self(this);
  job_queue_->Schedule([self]() { self->NotifyDestroyed(); });

  // Wait for the task to complete with a 1-second timeout.
  done_cv_.wait_for(lock, std::chrono::seconds(1), [this] { return done_; });
}

void SurfaceDestroyNotifier::NotifyDestroyed() {
  VideoSurfaceHolder* holder_to_notify = nullptr;
  {
    std::lock_guard lock(mutex_);
    if (!disconnected_) {
      holder_to_notify = holder_;
    }
  }

  if (holder_to_notify) {
    holder_to_notify->OnSurfaceDestroyed();
  }

  {
    std::lock_guard lock(mutex_);
    done_ = true;
  }
  done_cv_.notify_one();
}

// static
bool VideoSurfaceHolder::IsVideoSurfaceAvailable() {
  // We only consider video surface is available when there is a video
  // surface and it is not held by any decoder, i.e.
  // g_surface_destroy_notifier is nullptr.
  std::lock_guard lock(*GetViewSurfaceMutex());
  return !GetGlobalSurfaceDestroyNotifier() && GetGlobalVideoSurface();
}

VideoSurfaceHolder::AcquiredSurface VideoSurfaceHolder::AcquireVideoSurface(
    JobQueue* job_queue) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (GetGlobalSurfaceDestroyNotifier() != nullptr) {
    // Other VideoSurfaceHolder has already acquired destroyer
    return {};
  }
  if (!GetGlobalVideoSurface()) {
    // Video Surface not created yet
    return {};
  }
  GetGlobalSurfaceDestroyNotifier() =
      make_scoped_refptr<SurfaceDestroyNotifier>(this, job_queue);
  JNIEnv* env = jni_zero::AttachCurrentThread();

  return {GetGlobalSurfaceDestroyNotifier(),
          jni_zero::ScopedJavaLocalRef<jobject>(env, GetGlobalVideoSurface())};
}

void VideoSurfaceHolder::ReleaseVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());

  auto& notifier = GetGlobalSurfaceDestroyNotifier();
  if (!notifier || !notifier->IsCurrentHolder(this)) {
    return;
  }
  notifier->Disconnect();
  notifier = nullptr;
}

bool VideoSurfaceHolder::GetVideoWindowSize(int* width, int* height) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_native_video_window == nullptr) {
    return false;
  } else {
    *width = ANativeWindow_getWidth(g_native_video_window);
    *height = ANativeWindow_getHeight(g_native_video_window);
    return true;
  }
}

void VideoSurfaceHolder::CleanUpVideoWindow(
    bool force_clear,
    SbDecodeTargetGraphicsContextProvider* gpu_provider) {
  // Lock *GetViewSurfaceMutex() here, to avoid releasing g_native_video_window
  // during painting.
  std::lock_guard lock(*GetViewSurfaceMutex());

  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clean up video window when it was null.";
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    SB_LOG(INFO) << "Tried to clean up video window when JNIEnv was null.";
    return;
  }

  if (force_clear) {
    SB_CHECK(gpu_provider);
    gpu_provider->gles_context_runner(gpu_provider, &ClearNativeWindow,
                                      g_native_video_window);
    SB_LOG(INFO) << "Video surface has been cleared.";
    return;
  }

  SB_LOG(INFO) << "KJ: VideoSurfaceHolder::CleanUpVideoWindow -> calling "
                  "StarboardBridge::ResetVideoSurface()";
  StarboardBridge::GetInstance()->ResetVideoSurface(env);
  SB_LOG(INFO) << "Video surface has been reset (default behavior).";
}

void SetVideoSurfaceForTesting(JNIEnv* env, jobject surface) {
  JNI_VideoSurfaceView_OnVideoSurfaceChanged(
      env, jni_zero::JavaParamRef<jobject>(env, surface));
}

}  // namespace starboard
