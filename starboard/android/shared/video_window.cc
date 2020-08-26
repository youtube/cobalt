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

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
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
Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChanged(
    JNIEnv* env,
    jobject unused_this,
    jobject surface) {
  ScopedLock lock(*GetViewSurfaceMutex());
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
  SB_DCHECK(g_video_surface_holder == NULL);
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

void VideoSurfaceHolder::ClearVideoWindow() {
  // Lock *GetViewSurfaceMutex() here, to avoid releasing g_native_video_window
  // during painting.
  ScopedLock lock(*GetViewSurfaceMutex());

  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clear video window when it was null.";
    return;
  }

  if (g_reset_surface_on_clear_window) {
    int width = ANativeWindow_getWidth(g_native_video_window);
    int height = ANativeWindow_getHeight(g_native_video_window);
    if (width <= height) {
      JniEnvExt::Get()->CallStarboardVoidMethodOrAbort("resetVideoSurface",
                                                       "()V");
      return;
    }
  }

  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, NULL, NULL);
  if (display == EGL_NO_DISPLAY) {
    SB_DLOG(ERROR) << "Found no EGL display in ClearVideoWindow";
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
      0,
      EGL_NONE,
  };

  // First, query how many configs match the given attribute list.
  EGLint num_configs = 0;
  EGL_CALL(eglChooseConfig(display, kAttributeList, NULL, 0, &num_configs));
  SB_DCHECK(num_configs != 0);

  // Allocate space to receive the matching configs and retrieve them.
  EGLConfig* configs = new EGLConfig[num_configs];
  EGL_CALL(eglChooseConfig(display, kAttributeList, configs, num_configs,
                           &num_configs));

  EGLNativeWindowType native_window =
      static_cast<EGLNativeWindowType>(g_native_video_window);
  EGLConfig config;

  // Find the first config that successfully allow a window surface to be
  // created.
  EGLSurface surface;
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface = eglCreateWindowSurface(display, config, native_window, NULL);
    if (eglGetError() == EGL_SUCCESS)
      break;
  }
  if (surface == EGL_NO_SURFACE) {
    SB_DLOG(ERROR) << "Found no EGL surface in ClearVideoWindow";
    return;
  }
  SB_DCHECK(surface != EGL_NO_SURFACE);

  delete[] configs;

  // Create an OpenGL ES 2.0 context.
  EGLContext context = EGL_NO_CONTEXT;
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE,
  };
  context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib_list);
  SB_DCHECK(eglGetError() == EGL_SUCCESS);
  SB_DCHECK(context != EGL_NO_CONTEXT);

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
  EGL_CALL(eglTerminate(display));
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
