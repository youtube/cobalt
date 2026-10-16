// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_H_
#define MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "starboard/decode_target.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media {
// StarboardGpuFactory allows to post tasks on gpu thread.
// StarboardRenderer uses this class to post graphical tasks.
class StarboardGpuFactory : public gpu::CommandBufferStub::DestructionObserver {
 public:
  using GetStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*(base::UnguessableToken,
                                                      int32_t)>;
  StarboardGpuFactory();

  StarboardGpuFactory(const StarboardGpuFactory&) = delete;
  StarboardGpuFactory& operator=(const StarboardGpuFactory&) = delete;

  virtual ~StarboardGpuFactory();

  virtual void Initialize(base::UnguessableToken channel_token,
                          int32_t route_id,
                          base::OnceClosure callback) = 0;

  // Shared state for one RunSbDecodeTargetFunctionOnGpu() request. It is
  // reference counted so that a caller that gives up waiting (see
  // |abandoned|) can return while the task is still queued on the gpu thread
  // without the task touching freed memory.
  struct GlesClosureRun : public base::RefCountedThreadSafe<GlesClosureRun> {
    enum class Outcome {
      kPending,
      // |target_function| ran with the GL context current.
      kRan,
      // No command buffer stub (destroyed and could not be re-acquired).
      // Retrying will not help.
      kNoStub,
      // The stub exists but its GL context could not be made current. This
      // is usually transient.
      kContextNotCurrent,
    };

    GlesClosureRun();

    // Guards |abandoned| and the execution of |target_function| so that a
    // caller cannot return (destroying |target_function_context|) while the
    // closure is running.
    base::Lock lock;
    bool abandoned GUARDED_BY(lock) = false;
    Outcome outcome = Outcome::kPending;
    base::WaitableEvent done{base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED};

   private:
    friend class base::RefCountedThreadSafe<GlesClosureRun>;
    ~GlesClosureRun();
  };

  // Runs |target_function| on the gpu thread with the command buffer's GL
  // context made current, records the result in |run->outcome| and signals
  // |run->done|. If |run->abandoned| is already set the function is not run.
  // If the context cannot be made current |target_function| is NOT run; the
  // caller must check |run->outcome| rather than assume it ran, otherwise it
  // proceeds with an uninitialized decode target (b/565889635).
  virtual void RunSbDecodeTargetFunctionOnGpu(
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context,
      scoped_refptr<GlesClosureRun> run) = 0;
  virtual void RunCallbackOnGpu(base::OnceCallback<void()> callback,
                                base::WaitableEvent* done_event) = 0;
  virtual void PostCallbackToGpu(base::OnceCallback<void()> callback) = 0;
  virtual void CreateImageOnGpu(
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space,
      viz::SharedImageFormat format,
      scoped_refptr<gpu::ClientSharedImage>& shared_image,
      const std::vector<uint32_t>& texture_service_ids,
      const std::vector<uint32_t>& texture_targets,
      uint64_t decode_target,
#if BUILDFLAG(IS_ANDROID)
      scoped_refptr<gpu::RefCountedLock> drdc_lock,
#endif  // BUILDFLAG(IS_ANDROID)
      base::WaitableEvent* done_event) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_H_
