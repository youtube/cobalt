// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/testing/fake_graphics_context_provider.h"

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"

#if defined(ADDRESS_SANITIZER)
// By default, Leak Sanitizer and Address Sanitizer is expected to exist
// together. However, this is not true for all platforms.
// HAS_LEAK_SANTIZIER=0 explicitly removes the Leak Sanitizer from code.
#ifndef HAS_LEAK_SANITIZER
#define HAS_LEAK_SANITIZER 1
#endif  // HAS_LEAK_SANITIZER
#endif  // defined(ADDRESS_SANITIZER)

#if HAS_LEAK_SANITIZER
#include <sanitizer/lsan_interface.h>
#endif  // HAS_LEAK_SANITIZER

#include "starboard/configuration.h"
#include "starboard/memory.h"

#define EGL_CALL_PREFIX SbGetEglInterface()->

#define EGLConfig SbEglConfig
#define EGLint SbEglInt32
#define EGLNativeWindowType SbEglNativeWindowType

#define EGL_ALPHA_SIZE SB_EGL_ALPHA_SIZE
#define EGL_BLUE_SIZE SB_EGL_BLUE_SIZE
#define EGL_CONTEXT_CLIENT_VERSION SB_EGL_CONTEXT_CLIENT_VERSION
#define EGL_DEFAULT_DISPLAY SB_EGL_DEFAULT_DISPLAY
#define EGL_GREEN_SIZE SB_EGL_GREEN_SIZE
#define EGL_NONE SB_EGL_NONE
#define EGL_NO_CONTEXT SB_EGL_NO_CONTEXT
#define EGL_NO_DISPLAY SB_EGL_NO_DISPLAY
#define EGL_NO_SURFACE SB_EGL_NO_SURFACE
#define EGL_OPENGL_ES2_BIT SB_EGL_OPENGL_ES2_BIT
#define EGL_PBUFFER_BIT SB_EGL_PBUFFER_BIT
#define EGL_RED_SIZE SB_EGL_RED_SIZE
#define EGL_RENDERABLE_TYPE SB_EGL_RENDERABLE_TYPE
#define EGL_SUCCESS SB_EGL_SUCCESS
#define EGL_SURFACE_TYPE SB_EGL_SURFACE_TYPE
#define EGL_WINDOW_BIT SB_EGL_WINDOW_BIT

#define EGL_CALL(x)                                          \
  do {                                                       \
    EGL_CALL_PREFIX x;                                       \
    SB_DCHECK(EGL_CALL_PREFIX eglGetError() == EGL_SUCCESS); \
  } while (false)

#define EGL_CALL_SIMPLE(x) (EGL_CALL_PREFIX x)

namespace starboard {
namespace testing {

namespace {

#if SB_HAS(GLES2)
EGLint const kAttributeList[] = {EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 8,
                                 EGL_SURFACE_TYPE,
                                 EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_ES2_BIT,
                                 EGL_NONE};
#endif  // SB_HAS(GLES2)

}  // namespace

FakeGraphicsContextProvider::FakeGraphicsContextProvider()
    :
#if SB_HAS(GLES2)
      display_(EGL_NO_DISPLAY),
      surface_(EGL_NO_SURFACE),
      context_(EGL_NO_CONTEXT),
#endif  // SB_HAS(GLES2)
      window_(kSbWindowInvalid) {
  InitializeWindow();
#if SB_HAS(BLITTER)
  decoder_target_provider_.device = kSbBlitterInvalidDevice;
#elif SB_HAS(GLES2)
  InitializeEGL();
#endif  // SB_HAS(BLITTER)
}

FakeGraphicsContextProvider::~FakeGraphicsContextProvider() {
#if SB_HAS(GLES2)
  functor_queue_.Put(
      std::bind(&FakeGraphicsContextProvider::DestroyContext, this));
  functor_queue_.Wake();
  SbThreadJoin(decode_target_context_thread_, NULL);
  EGL_CALL(eglDestroySurface(display_, surface_));
  EGL_CALL(eglTerminate(display_));
#endif  // SB_HAS(GLES2)
  SbWindowDestroy(window_);
}

#if SB_HAS(GLES2)

void FakeGraphicsContextProvider::RunOnGlesContextThread(
    const std::function<void()>& functor) {
  if (SbThreadIsCurrent(decode_target_context_thread_)) {
    functor();
    return;
  }
  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);

  functor_queue_.Put(functor);
  functor_queue_.Put([&]() {
    ScopedLock scoped_lock(mutex);
    condition_variable.Signal();
  });
  condition_variable.Wait();
}

void FakeGraphicsContextProvider::ReleaseDecodeTarget(
    SbDecodeTarget decode_target) {
  if (SbThreadIsCurrent(decode_target_context_thread_)) {
    SbDecodeTargetRelease(decode_target);
    return;
  }

  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);

  functor_queue_.Put(std::bind(SbDecodeTargetRelease, decode_target));
  functor_queue_.Put([&]() {
    ScopedLock scoped_lock(mutex);
    condition_variable.Signal();
  });
  condition_variable.Wait();
}

// static
void* FakeGraphicsContextProvider::ThreadEntryPoint(void* context) {
  auto provider = static_cast<FakeGraphicsContextProvider*>(context);
  provider->RunLoop();

  return NULL;
}

void FakeGraphicsContextProvider::RunLoop() {
  while (std::function<void()> functor = functor_queue_.Get()) {
    if (!functor) {
      break;
    }
    functor();
  }
}

#endif  // SB_HAS(GLES2)

void FakeGraphicsContextProvider::InitializeWindow() {
  SbWindowOptions window_options;
  SbWindowSetDefaultOptions(&window_options);

  window_ = SbWindowCreate(&window_options);
  SB_CHECK(SbWindowIsValid(window_));
}

#if SB_HAS(GLES2)
void FakeGraphicsContextProvider::InitializeEGL() {
  display_ = EGL_CALL_SIMPLE(eglGetDisplay(EGL_DEFAULT_DISPLAY));
  SB_DCHECK(EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(EGL_NO_DISPLAY != display_);

#if HAS_LEAK_SANITIZER
  __lsan_disable();
#endif  // HAS_LEAK_SANITIZER
  EGL_CALL_SIMPLE(eglInitialize(display_, NULL, NULL));
#if HAS_LEAK_SANITIZER
  __lsan_enable();
#endif  // HAS_LEAK_SANITIZER
  SB_DCHECK(EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));

  // Some EGL drivers can return a first config that doesn't allow
  // eglCreateWindowSurface(), with no differences in EGLConfig attribute values
  // from configs that do allow that. To handle that, we have to attempt
  // eglCreateWindowSurface() until we find a config that succeeds.

  // First, query how many configs match the given attribute list.
  EGLint num_configs = 0;
  EGL_CALL(eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs));
  SB_CHECK(0 != num_configs);

  // Allocate space to receive the matching configs and retrieve them.
  EGLConfig* configs = reinterpret_cast<EGLConfig*>(
      SbMemoryAllocate(num_configs * sizeof(EGLConfig)));
  EGL_CALL(eglChooseConfig(display_, kAttributeList, configs, num_configs,
                           &num_configs));

  EGLNativeWindowType native_window =
      (EGLNativeWindowType)SbWindowGetPlatformHandle(window_);
  EGLConfig config = EGLConfig();

  // Find the first config that successfully allow a window surface to be
  // created.
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface_ = EGL_CALL_SIMPLE(
        eglCreateWindowSurface(display_, config, native_window, NULL));
    if (EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()))
      break;
  }
  SB_DCHECK(surface_ != EGL_NO_SURFACE);

  SbMemoryDeallocate(configs);

  // Create the GLES2 or GLEX3 Context.
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE,
  };
#if SB_API_VERSION < 12 && defined(GLES3_SUPPORTED)
  // Attempt to create an OpenGL ES 3.0 context.
  context_ = EGL_CALL_SIMPLE(eglCreateContext(
      display_, config, EGL_NO_CONTEXT, context_attrib_list));
#endif
  if (context_ == EGL_NO_CONTEXT) {
    // Create an OpenGL ES 2.0 context.
    context_attrib_list[1] = 2;
    context_ = EGL_CALL_SIMPLE(eglCreateContext(
        display_, config, EGL_NO_CONTEXT, context_attrib_list));
  }
  SB_CHECK(EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(context_ != EGL_NO_CONTEXT);

  MakeContextCurrent();

  decoder_target_provider_.egl_display = display_;
  decoder_target_provider_.egl_context = context_;
  decoder_target_provider_.gles_context_runner = DecodeTargetGlesContextRunner;
  decoder_target_provider_.gles_context_runner_context = this;

  decode_target_context_thread_ = SbThreadCreate(
      0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true, "dt_context",
      &FakeGraphicsContextProvider::ThreadEntryPoint, this);
  MakeNoContextCurrent();

  functor_queue_.Put(
      std::bind(&FakeGraphicsContextProvider::MakeContextCurrent, this));
}

void FakeGraphicsContextProvider::OnDecodeTargetGlesContextRunner(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  if (SbThreadIsCurrent(decode_target_context_thread_)) {
    target_function(target_function_context);
    return;
  }

  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);

  functor_queue_.Put(std::bind(target_function, target_function_context));
  functor_queue_.Put([&]() {
    ScopedLock scoped_lock(mutex);
    condition_variable.Signal();
  });
  condition_variable.Wait();
}

void FakeGraphicsContextProvider::MakeContextCurrent() {
  SB_CHECK(EGL_NO_DISPLAY != display_);
  EGL_CALL_SIMPLE(eglMakeCurrent(display_, surface_, surface_, context_));
  EGLint error = EGL_CALL_SIMPLE(eglGetError());
  SB_CHECK(EGL_SUCCESS == error) << " eglGetError " << error;
}

void FakeGraphicsContextProvider::MakeNoContextCurrent() {
  EGL_CALL(eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT));
}

void FakeGraphicsContextProvider::DestroyContext() {
  MakeNoContextCurrent();
  EGL_CALL_SIMPLE(eglDestroyContext(display_, context_));
  EGLint error = EGL_CALL_SIMPLE(eglGetError());
  SB_CHECK(EGL_SUCCESS == error) << " eglGetError " << error;
}

// static
void FakeGraphicsContextProvider::DecodeTargetGlesContextRunner(
    SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  FakeGraphicsContextProvider* provider =
      static_cast<FakeGraphicsContextProvider*>(
          graphics_context_provider->gles_context_runner_context);
  provider->OnDecodeTargetGlesContextRunner(target_function,
                                            target_function_context);
}

#endif  // SB_HAS(GLES2)

}  // namespace testing
}  // namespace starboard
