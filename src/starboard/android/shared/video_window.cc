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
#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Global pointer to the single video surface.
jobject g_j_video_surface = NULL;
// Global pointer to the single video window.
ANativeWindow* g_native_video_window = NULL;

// Facilitate synchronization of punch-out videos.
SbMutex g_bounds_updates_mutex = SB_MUTEX_INITIALIZER;
SbConditionVariable g_bounds_updates_condition =
    SB_CONDITION_VARIABLE_INITIALIZER;
int g_bounds_updates_needed = 0;
int g_bounds_updates_scheduled = 0;

}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChanged(
    JNIEnv* env,
    jobject unused_this,
    jobject surface) {
  if (g_j_video_surface) {
    // TODO: Ensure that the decoder isn't still using the surface.
    env->DeleteGlobalRef(g_j_video_surface);
  }
  if (g_native_video_window) {
    // TODO: Ensure that the decoder isn't still using the window.
    ANativeWindow_release(g_native_video_window);
  }
  if (surface) {
    g_j_video_surface = env->NewGlobalRef(surface);
    g_native_video_window = ANativeWindow_fromSurface(env, surface);
  } else {
    g_j_video_surface = NULL;
    g_native_video_window = NULL;
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnLayoutNeeded() {
  SbMutexAcquire(&g_bounds_updates_mutex);
  ++g_bounds_updates_needed;
  SbMutexRelease(&g_bounds_updates_mutex);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnLayoutScheduled() {
  SbMutexAcquire(&g_bounds_updates_mutex);
  ++g_bounds_updates_scheduled;
  SbMutexRelease(&g_bounds_updates_mutex);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_VideoSurfaceView_nativeOnGlobalLayout() {
  SbMutexAcquire(&g_bounds_updates_mutex);
  g_bounds_updates_needed -= g_bounds_updates_scheduled;
  g_bounds_updates_scheduled = 0;
  if (g_bounds_updates_needed <= 0) {
    g_bounds_updates_needed = 0;
    SbConditionVariableSignal(&g_bounds_updates_condition);
  }
  SbMutexRelease(&g_bounds_updates_mutex);
}

jobject GetVideoSurface() {
  return g_j_video_surface;
}

ANativeWindow* GetVideoWindow() {
  return g_native_video_window;
}

void ClearVideoWindow() {
  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clear video window when it was null.";
    return;
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

void WaitForVideoBoundsUpdate() {
  SbMutexAcquire(&g_bounds_updates_mutex);
  if (g_bounds_updates_needed > 0) {
    // Use a timed wait to deal with possible race conditions in which
    // suspend occurs in the middle of a bounds update.
    SbConditionVariableWaitTimed(&g_bounds_updates_condition,
                                 &g_bounds_updates_mutex,
                                 100 * kSbTimeMillisecond);
    g_bounds_updates_needed = 0;
    g_bounds_updates_scheduled = 0;
  }
  SbMutexRelease(&g_bounds_updates_mutex);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
