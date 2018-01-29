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

#include "starboard/shared/starboard/player/filter/fake_graphics_context_provider.h"

#include <functional>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {

FakeGraphicsContextProvider::FakeGraphicsContextProvider()
    : decode_target_context_thread_("dt_context") {
  decoder_target_provider_.gles_context_runner = DecodeTargetGlesContextRunner;
  decoder_target_provider_.gles_context_runner_context = this;
}

void FakeGraphicsContextProvider::ReleaseDecodeTarget(
    SbDecodeTarget decode_target) {
  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  ScopedLock scoped_lock(mutex);
  decode_target_context_thread_.job_queue()->Schedule(std::bind(
      &FakeGraphicsContextProvider::ReleaseDecodeTargetOnGlesContextThread,
      this, &mutex, &condition_variable, decode_target));
  condition_variable.Wait();
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
  decode_target_context_thread_.job_queue()->Schedule(std::bind(
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

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
