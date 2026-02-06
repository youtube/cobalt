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

#ifndef STARBOARD_TESTING_FAKE_GRAPHICS_CONTEXT_PROVIDER_H_
#define STARBOARD_TESTING_FAKE_GRAPHICS_CONTEXT_PROVIDER_H_

#include <pthread.h>

#include <functional>

#include "starboard/common/queue.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/window.h"

#include "starboard/egl.h"
#include "starboard/gles.h"

namespace starboard {

// This class provides a SbDecodeTargetGraphicsContextProvider implementation
// used by SbPlayer related tests.  It creates a thread and forwards decode
// target creation/destruction to the thread.
class FakeGraphicsContextProvider {
 public:
  FakeGraphicsContextProvider();
  ~FakeGraphicsContextProvider();

  SbWindow window() { return window_; }
  SbDecodeTargetGraphicsContextProvider* decoder_target_provider() {
    return &decoder_target_provider_;
  }

  void RunOnGlesContextThread(const std::function<void()>& functor);
  void ReleaseDecodeTarget(SbDecodeTarget decode_target);

  void Render();

 private:
  static void* ThreadEntryPoint(void* context);
  void RunLoop();

  void InitializeWindow();

  void InitializeEGL();

  void OnDecodeTargetGlesContextRunner(
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context);

  void MakeContextCurrent();
  void MakeNoContextCurrent();
  void DestroyContext();

  static void DecodeTargetGlesContextRunner(
      SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context);

  SbEglDisplay display_;
  SbEglSurface surface_;
  SbEglContext context_;
  Queue<std::function<void()>> functor_queue_;
  pthread_t decode_target_context_thread_;

  SbWindow window_;

  SbDecodeTargetGraphicsContextProvider decoder_target_provider_;
};

}  // namespace starboard

#endif  // STARBOARD_TESTING_FAKE_GRAPHICS_CONTEXT_PROVIDER_H_
