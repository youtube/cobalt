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
#define GL_CALL_PREFIX SbGetGlInterface()->

#define EGL_CALL(x)                                             \
  do {                                                          \
    EGL_CALL_PREFIX x;                                          \
    SB_DCHECK(EGL_CALL_PREFIX eglGetError() == SB_EGL_SUCCESS); \
  } while (false)

#define GL_CALL(x)                                            \
  do {                                                        \
    GL_CALL_PREFIX x;                                         \
    SB_DCHECK(GL_CALL_PREFIX glGetError() == SB_GL_NO_ERROR); \
  } while (false)

#define EGL_CALL_SIMPLE(x) (EGL_CALL_PREFIX x)
#define GL_CALL_SIMPLE(x) (GL_CALL_PREFIX x)

namespace starboard {
namespace testing {

namespace {

#if SB_HAS(GLES2)
SbEglInt32 const kAttributeList[] = {SB_EGL_RED_SIZE,
                                     8,
                                     SB_EGL_GREEN_SIZE,
                                     8,
                                     SB_EGL_BLUE_SIZE,
                                     8,
                                     SB_EGL_ALPHA_SIZE,
                                     8,
                                     SB_EGL_SURFACE_TYPE,
                                     SB_EGL_WINDOW_BIT | SB_EGL_PBUFFER_BIT,
                                     SB_EGL_RENDERABLE_TYPE,
                                     SB_EGL_OPENGL_ES2_BIT,
                                     SB_EGL_NONE};
#endif  // SB_HAS(GLES2)

}  // namespace

FakeGraphicsContextProvider::FakeGraphicsContextProvider()
    :
#if SB_HAS(GLES2)
      display_(SB_EGL_NO_DISPLAY),
      surface_(SB_EGL_NO_SURFACE),
      context_(SB_EGL_NO_CONTEXT),
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

void FakeGraphicsContextProvider::ReleaseDecodeTarget(
    SbDecodeTarget decode_target) {
  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);

  functor_queue_.Put(std::bind(
      &FakeGraphicsContextProvider::ReleaseDecodeTargetOnGlesContextThread,
      this, &mutex, &condition_variable, decode_target));
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
  display_ = EGL_CALL_SIMPLE(eglGetDisplay(SB_EGL_DEFAULT_DISPLAY));
  SB_DCHECK(SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(SB_EGL_NO_DISPLAY != display_);

#if HAS_LEAK_SANITIZER
  __lsan_disable();
#endif  // HAS_LEAK_SANITIZER
  EGL_CALL_SIMPLE(eglInitialize(display_, NULL, NULL));
#if HAS_LEAK_SANITIZER
  __lsan_enable();
#endif  // HAS_LEAK_SANITIZER
  SB_DCHECK(SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));

  // Some EGL drivers can return a first config that doesn't allow
  // eglCreateWindowSurface(), with no differences in EGLConfig attribute values
  // from configs that do allow that. To handle that, we have to attempt
  // eglCreateWindowSurface() until we find a config that succeeds.

  // First, query how many configs match the given attribute list.
  SbEglInt32 num_configs = 0;
  EGL_CALL(eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs));
  SB_CHECK(0 != num_configs);

  // Allocate space to receive the matching configs and retrieve them.
  SbEglConfig* configs = reinterpret_cast<SbEglConfig*>(
      SbMemoryAllocate(num_configs * sizeof(SbEglConfig)));
  EGL_CALL(eglChooseConfig(display_, kAttributeList, configs, num_configs,
                           &num_configs));

  SbEglNativeWindowType native_window =
      (SbEglNativeWindowType)SbWindowGetPlatformHandle(window_);
  SbEglConfig config = SbEglConfig();

  // Find the first config that successfully allow a window surface to be
  // created.
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface_ = EGL_CALL_SIMPLE(
        eglCreateWindowSurface(display_, config, native_window, NULL));
    if (SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()))
      break;
  }
  SB_DCHECK(surface_ != SB_EGL_NO_SURFACE);

  SbMemoryDeallocate(configs);

  // Create the GLES2 or GLEX3 Context.
  SbEglInt32 context_attrib_list[] = {
      SB_EGL_CONTEXT_CLIENT_VERSION, 3, SB_EGL_NONE,
  };
#if defined(GLES3_SUPPORTED)
  // Attempt to create an OpenGL ES 3.0 context.
  context_ = EGL_CALL_SIMPLE(eglCreateContext(
      display_, config, SB_EGL_NO_CONTEXT, context_attrib_list));
#endif
  if (context_ == SB_EGL_NO_CONTEXT) {
    // Create an OpenGL ES 2.0 context.
    context_attrib_list[1] = 2;
    context_ = EGL_CALL_SIMPLE(eglCreateContext(
        display_, config, SB_EGL_NO_CONTEXT, context_attrib_list));
  }
  SB_CHECK(SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(context_ != SB_EGL_NO_CONTEXT);

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

void FakeGraphicsContextProvider::ReleaseDecodeTargetOnGlesContextThread(
    Mutex* mutex,
    ConditionVariable* condition_variable,
    SbDecodeTarget decode_target) {
  SbDecodeTargetRelease(decode_target);
  ScopedLock scoped_lock(*mutex);
  condition_variable->Signal();
}

void FakeGraphicsContextProvider::RunDecodeTargetFunctionOnGlesContextThread(
    Mutex* mutex,
    ConditionVariable* condition_variable,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  target_function(target_function_context);
  ScopedLock scoped_lock(*mutex);
  condition_variable->Signal();
}

void FakeGraphicsContextProvider::OnDecodeTargetGlesContextRunner(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);

  functor_queue_.Put(std::bind(
      &FakeGraphicsContextProvider::RunDecodeTargetFunctionOnGlesContextThread,
      this, &mutex, &condition_variable, target_function,
      target_function_context));
  condition_variable.Wait();
}

void FakeGraphicsContextProvider::MakeContextCurrent() {
  SB_CHECK(SB_EGL_NO_DISPLAY != display_);
  EGL_CALL_SIMPLE(eglMakeCurrent(display_, surface_, surface_, context_));
  SbEglInt32 error = EGL_CALL_SIMPLE(eglGetError());
  SB_CHECK(SB_EGL_SUCCESS == error) << " eglGetError " << error;
}

void FakeGraphicsContextProvider::MakeNoContextCurrent() {
  EGL_CALL(eglMakeCurrent(display_, SB_EGL_NO_SURFACE, SB_EGL_NO_SURFACE,
                          SB_EGL_NO_CONTEXT));
}

void FakeGraphicsContextProvider::DestroyContext() {
  MakeNoContextCurrent();
  EGL_CALL_SIMPLE(eglDestroyContext(display_, context_));
  SbEglInt32 error = EGL_CALL_SIMPLE(eglGetError());
  SB_CHECK(SB_EGL_SUCCESS == error) << " eglGetError " << error;
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
