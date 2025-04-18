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
#include "starboard/common/log.h"
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

#define EGL_CALL_PREFIX SbGetEglInterface()->

#define EGLConfig SbEglConfig
#define EGLint SbEglInt32
#define EGLNativeWindowType SbEglNativeWindowType

#define EGL_ALPHA_SIZE SB_EGL_ALPHA_SIZE
#define EGL_BLUE_SIZE SB_EGL_BLUE_SIZE
#define EGL_CONTEXT_CLIENT_VERSION SB_EGL_CONTEXT_CLIENT_VERSION
#define EGL_DEFAULT_DISPLAY SB_EGL_DEFAULT_DISPLAY
#define EGL_GREEN_SIZE SB_EGL_GREEN_SIZE
#define EGL_HEIGHT SB_EGL_HEIGHT
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
#define EGL_WIDTH SB_EGL_WIDTH
#define EGL_WINDOW_BIT SB_EGL_WINDOW_BIT

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif /* EGL_ANGLE_platform_angle_opengl */

#define EGL_CALL(x)                                          \
  do {                                                       \
    EGL_CALL_PREFIX x;                                       \
    SB_DCHECK(EGL_CALL_PREFIX eglGetError() == EGL_SUCCESS); \
  } while (false)

#define EGL_CALL_SIMPLE(x) (EGL_CALL_PREFIX x)

#define GL_CALL(x)                                                     \
  do {                                                                 \
    SbGetGlesInterface()->x;                                           \
    SB_DCHECK((SbGetGlesInterface()->glGetError()) == SB_GL_NO_ERROR); \
  } while (false)

namespace starboard {
namespace testing {

FakeGraphicsContextProvider::FakeGraphicsContextProvider()
    : display_(EGL_NO_DISPLAY),
      surface_(EGL_NO_SURFACE),
      context_(EGL_NO_CONTEXT),
      window_(kSbWindowInvalid) {
  InitializeWindow();
  InitializeEGL();
}

FakeGraphicsContextProvider::~FakeGraphicsContextProvider() {
  functor_queue_.Put(
      std::bind(&FakeGraphicsContextProvider::DestroyContext, this));
  functor_queue_.Wake();
  pthread_join(decode_target_context_thread_, NULL);
  EGL_CALL(eglDestroySurface(display_, surface_));
  EGL_CALL(eglTerminate(display_));
  SbWindowDestroy(window_);
}

void FakeGraphicsContextProvider::RunOnGlesContextThread(
    const std::function<void()>& functor) {
  if (pthread_equal(pthread_self(), decode_target_context_thread_)) {
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
  if (pthread_equal(pthread_self(), decode_target_context_thread_)) {
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
  pthread_setname_np(pthread_self(), "dt_context");
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

void FakeGraphicsContextProvider::InitializeWindow() {
  SbWindowOptions window_options;
  SbWindowSetDefaultOptions(&window_options);

  window_ = SbWindowCreate(&window_options);
  SB_CHECK(SbWindowIsValid(window_));
}

void FakeGraphicsContextProvider::InitializeEGL() {
  std::vector<SbEglAttrib> display_attribs = {
      EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE, EGL_PLATFORM_ANGLE_TYPE_ANGLE,
      EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
      EGL_NONE  // Terminate the attribute list
  };
#if BUILDFLAG(IS_ANDROID)
  display_ = EGL_CALL_SIMPLE(eglGetDisplay(EGL_DEFAULT_DISPLAY));
#else
  display_ = EGL_CALL_SIMPLE(eglGetPlatformDisplay(
      EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY),
      display_attribs.data()));
#endif  // BUILDFLAG(IS_ANDROID)

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

  constexpr EGLint kAttributeList[] = {EGL_RED_SIZE,
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

  // First, query how many configs match the given attribute list.
  EGLint num_configs = 0;
  EGL_CALL(eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs));
  SB_CHECK(0 != num_configs);

  // Allocate space to receive the matching configs and retrieve them.
  std::vector<EGLConfig> configs(num_configs);
  EGL_CALL(eglChooseConfig(display_, kAttributeList, configs.data(),
                           num_configs, &num_configs));
  // "If configs is not NULL, up to config_size configs will be returned in the
  // array pointed to by configs. The number of configs actually returned will
  // be returned in *num_config." Assert that and resize if needed.
  SB_CHECK(num_configs <= configs.size());
  configs.resize(num_configs);

  // Find the first config that successfully allows a pBuffer surface (i.e. an
  // offscreen EGLsurface) to be created.
  EGLConfig config = EGLConfig();
  for (auto maybe_config : configs) {
    constexpr EGLint kPBufferAttribs[] = {EGL_WIDTH, 1920, EGL_HEIGHT, 1080,
                                          EGL_NONE};
    surface_ = EGL_CALL_SIMPLE(
        eglCreatePbufferSurface(display_, maybe_config, kPBufferAttribs));
    if (EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError())) {
      config = maybe_config;
      break;
    }
  }
  SB_DCHECK(surface_ != EGL_NO_SURFACE);

  // Create the GLES2 or GLES3 Context.
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION,
      3,
      EGL_NONE,
  };
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

  pthread_create(&decode_target_context_thread_, nullptr,
                 &FakeGraphicsContextProvider::ThreadEntryPoint, this);
  MakeNoContextCurrent();

  functor_queue_.Put(
      std::bind(&FakeGraphicsContextProvider::MakeContextCurrent, this));
}

void FakeGraphicsContextProvider::OnDecodeTargetGlesContextRunner(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  if (pthread_equal(pthread_self(), decode_target_context_thread_)) {
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
  EGL_CALL(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

void FakeGraphicsContextProvider::Render() {
  GL_CALL(glClear(SB_GL_COLOR_BUFFER_BIT));
  EGL_CALL(eglSwapBuffers(display_, surface_));
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

}  // namespace testing
}  // namespace starboard
