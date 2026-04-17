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

#include <atomic>
#include <functional>
#include <memory>

#include "starboard/common/queue.h"
#include "starboard/common/thread.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/egl.h"
#include "starboard/gles.h"
#include "starboard/window.h"

namespace starboard {

// This class provides a SbDecodeTargetGraphicsContextProvider implementation
// used by SbPlayer related tests.  It creates a thread and forwards decode
// target creation/destruction to the thread.
class FakeGraphicsContextProvider {
 public:
  FakeGraphicsContextProvider();
  ~FakeGraphicsContextProvider();

  SbWindow window() { return kSbWindowInvalid; }
  SbDecodeTargetGraphicsContextProvider* decoder_target_provider() {
    return &decoder_target_provider_;
  }

  void RunOnGlesContextThread(const std::function<void()>& functor);
  void ReleaseDecodeTarget(SbDecodeTarget decode_target);

  void Render();

 private:
  class GlesContextThread : public Thread {
   public:
    explicit GlesContextThread(FakeGraphicsContextProvider* provider)
        : Thread("dt_context"), provider_(provider) {}
    void Run() override { provider_->RunLoop(); }

   private:
    FakeGraphicsContextProvider* provider_;
  };

  void RunLoop();

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

  std::unique_ptr<GlesContextThread> gles_context_thread_;
  std::atomic<SbThreadId> gles_context_thread_id_{kSbThreadInvalidId};

  SbDecodeTargetGraphicsContextProvider decoder_target_provider_;
};

}  // namespace starboard

#endif  // STARBOARD_TESTING_FAKE_GRAPHICS_CONTEXT_PROVIDER_H_
