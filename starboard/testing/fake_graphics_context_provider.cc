// Copyright 2018 Google Inc. All Rights Reserved.
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

namespace starboard {
namespace testing {

FakeGraphicsContextProvider::FakeGraphicsContextProvider() {
#if SB_HAS(BLITTER)
  decoder_target_provider_.device = kSbBlitterInvalidDevice;
#elif SB_HAS(GLES2)
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  SB_CHECK(EGL_NO_DISPLAY != display_);

#if HAS_LEAK_SANITIZER
  __lsan_disable();
#endif  // HAS_LEAK_SANITIZER
  eglInitialize(display_, NULL, NULL);
#if HAS_LEAK_SANITIZER
  __lsan_enable();
#endif  // HAS_LEAK_SANITIZER

  SB_CHECK(EGL_SUCCESS == eglGetError());
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  SB_CHECK(EGL_SUCCESS == eglGetError());

  decoder_target_provider_.egl_display = display_;
  // TODO: Set a valid context.
  decoder_target_provider_.egl_context = NULL;
  decoder_target_provider_.gles_context_runner = DecodeTargetGlesContextRunner;
  decoder_target_provider_.gles_context_runner_context = this;

  decode_target_context_thread_ = SbThreadCreate(
      0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true, "dt_context",
      &FakeGraphicsContextProvider::ThreadEntryPoint, this);
#endif  // SB_HAS(BLITTER)
}

FakeGraphicsContextProvider::~FakeGraphicsContextProvider() {
#if SB_HAS(GLES2)
  functor_queue_.Wake();
  SbThreadJoin(decode_target_context_thread_, NULL);
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  eglTerminate(display_);
  SB_CHECK(EGL_SUCCESS == eglGetError());
#endif  // SB_HAS(GLES2)
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
