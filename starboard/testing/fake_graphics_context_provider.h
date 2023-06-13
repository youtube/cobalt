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

#include <functional>

#include "starboard/common/queue.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/thread.h"
#include "starboard/window.h"

#include "starboard/egl.h"
#include "starboard/gles.h"

namespace starboard {
namespace testing {

// This class provides a SbDecodeTargetGraphicsContextProvider implementation
// used by SbPlayer related tests.  It creates a thread and forwards decode
// target creation/destruction to the thread.
class FakeGraphicsContextProvider {
 public:
  FakeGraphicsContextProvider();
  ~FakeGraphicsContextProvider();

  SbWindow window() { return window_; }
  SbDecodeTargetGraphicsContextProvider* decoder_target_provider() {
#if SB_HAS(GLES2)
    return &decoder_target_provider_;
#else   // SB_HAS(GLES2)
    return NULL;
#endif  // SB_HAS(GLES2)
  }

#if SB_HAS(GLES2)
  void RunOnGlesContextThread(const std::function<void()>& functor);
  void ReleaseDecodeTarget(SbDecodeTarget decode_target);
#endif  // SB_HAS(GLES2)

  void Render();

 private:
#if SB_HAS(GLES2)
  static void* ThreadEntryPoint(void* context);
  void RunLoop();
#endif  // SB_HAS(GLES2)

  void InitializeWindow();

#if SB_HAS(GLES2)
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
  SbThread decode_target_context_thread_;
#endif  // SB_HAS(GLES2)

  SbWindow window_;

#if SB_HAS(GLES2)
  SbDecodeTargetGraphicsContextProvider decoder_target_provider_;
#endif  // SB_HAS(GLES2)
};

}  // namespace testing
}  // namespace starboard

#endif  // STARBOARD_TESTING_FAKE_GRAPHICS_CONTEXT_PROVIDER_H_
