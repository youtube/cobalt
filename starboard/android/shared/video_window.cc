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
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>
#include <strings.h>
#include <sys/system_properties.h>

#include <mutex>
#include <set>
#include <vector>

#include "cobalt/android/jni_headers/VideoSurfaceView_jni.h"
#include "starboard/android/shared/protected_video_plane.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"
#include "starboard/common/once.h"
#include "starboard/configuration.h"
#include "starboard/shared/gles/gl_call.h"
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
// Global video surface pointer holder.
VideoSurfaceHolder* g_video_surface_holder = nullptr;

// Holder used while a protected GL presenter owns the video window (see
// AcquireVideoWindowForPresenter()). While it is installed, decoders cannot
// acquire the surface and CleanUpVideoSurface() leaves the window alone, so
// exactly one EGL producer is connected to the window.
class PresenterSurfaceHolder final : public VideoSurfaceHolder {
 public:
  void OnSurfaceDestroyed() override {
    SB_LOG(WARNING) << "Video surface destroyed while a protected presenter "
                       "holds it.";
  }
};
PresenterSurfaceHolder* GetPresenterSurfaceHolder() {
  static NoDestructor<PresenterSurfaceHolder> holder;
  return holder.get();
}

struct PresenterObservers {
  std::mutex mutex;
  std::set<ProtectedPresenterObserver*> observers;
};
PresenterObservers& GetPresenterObservers() {
  static NoDestructor<PresenterObservers> observers;
  return *observers;
}

// True while CleanUpVideoSurface() clears the window outside the mutex; no
// presenter may connect to it meanwhile. Guarded by GetViewSurfaceMutex().
bool g_clearing_video_window = false;

void ClearNativeWindow(void* raw_context) {
  ANativeWindow* native_window = static_cast<ANativeWindow*>(raw_context);
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    SB_LOG(ERROR) << "Found no EGL display in ClearNativeWindow";
    return;
  }

  // Save pre-existing EGL context state to restore at the end of this function.
  // This prevents unbinding the shared GPU thread's context from other active
  // players.
  EGLDisplay old_display = eglGetCurrentDisplay();
  EGLSurface old_draw_surface = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface old_read_surface = eglGetCurrentSurface(EGL_READ);
  EGLContext old_context = eglGetCurrentContext();

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

  // Restore pre-existing EGL state and destroy temporary cleanup resources.
  EGL_CALL(eglMakeCurrent(old_context == EGL_NO_CONTEXT ? display : old_display,
                          old_draw_surface, old_read_surface, old_context));
  EGL_CALL(eglDestroyContext(display, context));
  EGL_CALL(eglDestroySurface(display, surface));
}

}  // namespace

void JNI_VideoSurfaceView_OnVideoSurfaceChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& surface) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder) {
    g_video_surface_holder->OnSurfaceDestroyed();
    g_video_surface_holder = NULL;
  }
  GetGlobalVideoSurface().Reset();
  if (g_native_video_window) {
    ANativeWindow_release(g_native_video_window);
    g_native_video_window = NULL;
  }
  if (surface) {
    GetGlobalVideoSurface().Reset(env, surface);
    g_native_video_window = ANativeWindow_fromSurface(env, surface.obj());
  }
}

// static
bool VideoSurfaceHolder::IsVideoSurfaceAvailable() {
  // We only consider video surface is available when there is a video
  // surface and it is not held by any decoder, i.e.
  // g_video_surface_holder is NULL.
  std::lock_guard lock(*GetViewSurfaceMutex());
  return !g_video_surface_holder && GetGlobalVideoSurface();
}

jni_zero::ScopedJavaLocalRef<jobject>
VideoSurfaceHolder::AcquireVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder != NULL) {
    return {};
  }
  if (!GetGlobalVideoSurface()) {
    return {};
  }
  g_video_surface_holder = this;
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return jni_zero::ScopedJavaLocalRef<jobject>(env, GetGlobalVideoSurface());
}

void VideoSurfaceHolder::ReleaseVideoSurface() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder == this) {
    g_video_surface_holder = NULL;
  }
}

void VideoSurfaceHolder::CleanUpVideoSurface(
    SbDecodeTargetGraphicsContextProvider* gpu_provider) {
  // Lock *GetViewSurfaceMutex() here, to avoid releasing g_native_video_window
  // during painting.
  std::unique_lock lock(*GetViewSurfaceMutex());

  if (!g_native_video_window) {
    SB_LOG(INFO) << "Tried to clean up video window when it was null.";
    return;
  }
  if (g_video_surface_holder == GetPresenterSurfaceHolder()) {
    SB_LOG(INFO) << "Not clearing the video window: a protected presenter "
                    "holds it.";
    return;
  }

  SB_CHECK(gpu_provider);
  // The runner blocks on the GPU thread, which takes this mutex to acquire the
  // window for a protected presenter: running it under the lock would
  // deadlock. Pin the window and fence presenters off instead.
  ANativeWindow* window = g_native_video_window;
  ANativeWindow_acquire(window);
  g_clearing_video_window = true;
  lock.unlock();
  gpu_provider->gles_context_runner(gpu_provider, &ClearNativeWindow, window);
  lock.lock();
  g_clearing_video_window = false;
  ANativeWindow_release(window);
  SB_LOG(INFO) << "Video surface has been cleared.";
}

void* AcquireVideoWindowForPresenter() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_clearing_video_window) {
    SB_LOG(WARNING) << "Video window is being cleared by a decoder.";
    return nullptr;
  }
  if (!g_native_video_window) {
    SB_LOG(WARNING) << "No video window for the protected presenter.";
    return nullptr;
  }
  if (g_video_surface_holder != nullptr &&
      g_video_surface_holder != GetPresenterSurfaceHolder()) {
    SB_LOG(WARNING) << "Video window is held by a decoder.";
    return nullptr;
  }
  if (g_video_surface_holder == GetPresenterSurfaceHolder()) {
    SB_LOG(WARNING) << "Video window is already held by a presenter.";
    return nullptr;
  }
  g_video_surface_holder = GetPresenterSurfaceHolder();
  ANativeWindow_acquire(g_native_video_window);
  SB_LOG(INFO) << "Protected presenter acquired the video window.";
  SetVideoSurfaceFullscreen();
  return g_native_video_window;
}

namespace {

// MediaTek's composer counts a buffer as secure only if its usage carries
// AHARDWAREBUFFER_USAGE_VENDOR_0 as well as PROTECTED_CONTENT (it logs
// "is_protected 1 != is_secured 0" otherwise). Its secure decoder sets both on
// its own output buffers. The allocator already places PROTECTED_CONTENT alone
// in the secure video heap, so this is about agreeing with the composer, not
// about where the memory lives.
bool UsesMediaTekSecureUsageBit() {
  static const bool uses = [] {
    char value[PROP_VALUE_MAX] = {};
    __system_property_get("ro.soc.manufacturer", value);
    if (strcasecmp(value, "mediatek") != 0) {
      return false;
    }
    // Respect an explicit "no secure video path"; the property may also be
    // unreadable from the app's SELinux domain, which reads as empty.
    char svp[PROP_VALUE_MAX] = {};
    __system_property_get("ro.vendor.mtk_sec_video_path_support", svp);
    return strcmp(svp, "0") != 0;
  }();
  return uses;
}

}  // namespace

void PrepareVideoWindowSurface(void* window) {
  if (!window || !UsesMediaTekSecureUsageBit()) {
    return;
  }
  // ANativeWindow_setUsage() is in libnativewindow (LL-NDK), not in the NDK
  // headers. It sets the producer usage of buffers allocated from now on, so it
  // must run after eglCreateWindowSurface() (which sets its own usage) and
  // before the first dequeue. The value keeps what EGL requested for a
  // protected GPU window surface and adds the vendor bit.
  using SetUsageFn = int (*)(ANativeWindow*, uint64_t);
  static SetUsageFn set_usage = [] {
    void* lib = dlopen("libnativewindow.so", RTLD_NOW | RTLD_NOLOAD);
    return lib ? reinterpret_cast<SetUsageFn>(
                     dlsym(lib, "ANativeWindow_setUsage"))
               : nullptr;
  }();
  constexpr uint64_t usage = AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT |
                             AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                             AHARDWAREBUFFER_USAGE_VENDOR_0;
  const int result =
      set_usage ? set_usage(static_cast<ANativeWindow*>(window), usage) : -1;
  if (result == 0) {
    SB_LOG(INFO) << "Protected presenter window: buffer usage 0x" << std::hex
                 << usage << " (MediaTek secure bit).";
  } else {
    SB_LOG(WARNING) << "Protected presenter window: could not set the "
                       "MediaTek secure usage bit ("
                    << (set_usage ? "error " : "no ANativeWindow_setUsage ")
                    << result << ").";
  }
}

bool IsVideoWindowHeldByPresenter() {
  std::lock_guard lock(*GetViewSurfaceMutex());
  return g_video_surface_holder == GetPresenterSurfaceHolder();
}

void ReleaseVideoWindowForPresenter(void* window) {
  std::lock_guard lock(*GetViewSurfaceMutex());
  if (g_video_surface_holder == GetPresenterSurfaceHolder()) {
    g_video_surface_holder = nullptr;
  }
  if (window) {
    ANativeWindow_release(static_cast<ANativeWindow*>(window));
  }
  SB_LOG(INFO) << "Protected presenter released the video window.";
  // Back to the hole geometry for whoever renders next, and let protected
  // decode-to-texture players fall back to punch-out (they re-check later, so
  // a presenter re-attaching right away keeps the protected path).
  ReapplyVideoSurfaceBounds();
  std::lock_guard observers_lock(GetPresenterObservers().mutex);
  for (ProtectedPresenterObserver* observer :
       GetPresenterObservers().observers) {
    observer->OnProtectedPresenterReleased();
  }
}

void AddProtectedPresenterObserver(ProtectedPresenterObserver* observer) {
  std::lock_guard lock(GetPresenterObservers().mutex);
  GetPresenterObservers().observers.insert(observer);
}

void RemoveProtectedPresenterObserver(ProtectedPresenterObserver* observer) {
  std::lock_guard lock(GetPresenterObservers().mutex);
  GetPresenterObservers().observers.erase(observer);
}

void VideoSurfaceHolder::ResetVideoSurface() {
  // Lock *GetViewSurfaceMutex() here, to avoid releasing g_native_video_window
  // during painting.
  std::lock_guard lock(*GetViewSurfaceMutex());

  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    SB_LOG(INFO) << "Tried to reset video window when JNIEnv was null.";
    return;
  }

  StarboardBridge::GetInstance()->ResetVideoSurface(env);
  SB_LOG(INFO) << "Video surface has been reset.";
}

}  // namespace starboard
