// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_ablation.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gl/gl_bindings.h"

namespace cobalt {

namespace {

constexpr char kGpuResultHistogram[] =
    "Cobalt.Features.GpuMemoryAblation.Result";

std::atomic_bool g_was_applied{false};
std::atomic_bool g_gpu_was_applied{false};

// Holds allocated buffers alive for the duration of the process.
std::vector<std::unique_ptr<char[]>>& GetAblatedMemoryStore() {
  static base::NoDestructor<std::vector<std::unique_ptr<char[]>>> store;
  return *store;
}

// Holds allocated GL buffer object names alive for the duration of the
// process. The ablation runs at most once and the buffers are intentionally
// never freed; the driver reclaims them when the context or process dies.
std::vector<GLuint>& GetAblatedGpuBufferStore() {
  static base::NoDestructor<std::vector<GLuint>> store;
  return *store;
}

// Drains pending GL error flags. glGetError() returns and clears one error
// flag per call, and GL keeps at most one flag per distinct error code, so a
// conforming driver needs only a handful of iterations. The bound guards
// against a misbehaving driver that keeps reporting errors indefinitely.
void DrainGlErrors(gl::GLApi* api) {
  constexpr int kMaxGlErrorDrainIterations = 16;
  bool drained = false;
  for (int i = 0; i < kMaxGlErrorDrainIterations; ++i) {
    const GLenum error = api->glGetErrorFn();
    if (error == GL_NO_ERROR || error == GL_CONTEXT_LOST_KHR) {
      drained = true;
      break;
    }
  }
  // Records whether errors were still pending after the bounded loop, which
  // would indicate unexpected driver behavior that may skew experiment results.
  base::UmaHistogramBoolean(
      "Cobalt.Features.GpuMemoryAblation.GlErrorDrainExhausted", !drained);
}

GpuMemoryAblationResult GlErrorToResult(GLenum error) {
  switch (error) {
    case GL_OUT_OF_MEMORY:
      return GpuMemoryAblationResult::kGlOutOfMemory;
    case GL_CONTEXT_LOST_KHR:
      return GpuMemoryAblationResult::kContextLost;
    default:
      return GpuMemoryAblationResult::kGlOtherError;
  }
}

void StoreAblatedMemory(std::unique_ptr<char[]> buffer) {
  GetAblatedMemoryStore().push_back(std::move(buffer));
  base::UmaHistogramEnumeration("Cobalt.Features.NativeMemoryAblation.Result",
                                NativeMemoryAblationResult::kSuccess);
}

void DoMemoryAblationInBackground(
    int size_mb,
    scoped_refptr<base::SequencedTaskRunner> reply_runner) {
  const size_t bytes_to_allocate = static_cast<size_t>(size_mb) * 1024 * 1024;
  constexpr size_t kPageSize = 4096;

  auto buffer =
      std::unique_ptr<char[]>(new (std::nothrow) char[bytes_to_allocate]);
  if (!buffer) {
    base::UmaHistogramEnumeration("Cobalt.Features.NativeMemoryAblation.Result",
                                  NativeMemoryAblationResult::kOomFailure);
    return;
  }

  // Touch each 4096-byte page using a volatile pointer so the compiler does
  // not optimize away the writes and the OS actually commits physical RAM pages
  // (RSS).
  volatile char* raw_ptr = buffer.get();
  for (size_t i = 0; i < bytes_to_allocate; i += kPageSize) {
    raw_ptr[i] = static_cast<char>(i & 0xFF);
  }

  if (reply_runner && reply_runner->RunsTasksInCurrentSequence()) {
    StoreAblatedMemory(std::move(buffer));
  } else if (reply_runner) {
    reply_runner->PostTask(
        FROM_HERE, base::BindOnce(&StoreAblatedMemory, std::move(buffer)));
  } else {
    StoreAblatedMemory(std::move(buffer));
  }
}

// Allocates |size_mb| MB of GPU memory on the shared GL context. Must run on
// the GPU main thread.
GpuMemoryAblationResult AllocateGpuMemoryOnGpuThread(
    base::WeakPtr<gpu::GpuChannelManager> channel_manager,
    int size_mb) {
  if (!channel_manager) {
    return GpuMemoryAblationResult::kChannelManagerUnavailable;
  }

  gpu::ContextResult result = gpu::ContextResult::kTransientFailure;
  scoped_refptr<gpu::SharedContextState> shared_context_state =
      channel_manager->GetSharedContextState(&result);
  if (!shared_context_state) {
    LOG(WARNING) << "GPU memory ablation: SharedContextState unavailable.";
    return GpuMemoryAblationResult::kContextUnavailable;
  }
  if (shared_context_state->context_lost()) {
    LOG(WARNING) << "GPU memory ablation: SharedContextState lost.";
    return GpuMemoryAblationResult::kContextLost;
  }
  if (!shared_context_state->MakeCurrent(nullptr, /*needs_gl=*/true)) {
    LOG(WARNING) << "GPU memory ablation: MakeCurrent failed.";
    return GpuMemoryAblationResult::kMakeCurrentFailed;
  }

  gl::GLApi* api = shared_context_state->context_state()
                       ? shared_context_state->context_state()->api()
                       : nullptr;
  if (!api) {
    LOG(WARNING) << "GPU memory ablation: GL API unavailable.";
    return GpuMemoryAblationResult::kGlApiUnavailable;
  }

  // Allocate in 1 MiB GL_ARRAY_BUFFER chunks using core GLES 2.0 / 3.0 APIs.
  constexpr size_t kChunkBytes = 1024 * 1024;
  std::vector<uint8_t> dirty_bytes(kChunkBytes, 0xA5);

  DrainGlErrors(api);

  std::vector<GLuint> allocated_buffers;
  allocated_buffers.reserve(size_mb);

  for (int i = 0; i < size_mb; ++i) {
    GLuint buffer_id = 0;
    api->glGenBuffersARBFn(1, &buffer_id);
    api->glBindBufferFn(GL_ARRAY_BUFFER, buffer_id);

    // Upload non-zero data so the GPU driver commits physical pages.
    api->glBufferDataFn(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kChunkBytes),
                        dirty_bytes.data(), GL_STATIC_DRAW);

    const GLenum error = api->glGetErrorFn();
    if (error != GL_NO_ERROR) {
      LOG(WARNING) << "GPU memory ablation failed with GL error 0x" << std::hex
                   << error << std::dec << " at chunk " << i << " of "
                   << size_mb << " MB.";
      api->glDeleteBuffersARBFn(1, &buffer_id);
      if (!allocated_buffers.empty()) {
        api->glDeleteBuffersARBFn(
            static_cast<GLsizei>(allocated_buffers.size()),
            allocated_buffers.data());
      }
      api->glBindBufferFn(GL_ARRAY_BUFFER, 0);
      DrainGlErrors(api);
      shared_context_state->PessimisticallyResetGrContext();
      shared_context_state->set_need_context_state_reset(true);
      return GlErrorToResult(error);
    }
    allocated_buffers.push_back(buffer_id);
  }

  api->glBindBufferFn(GL_ARRAY_BUFFER, 0);
  api->glFlushFn();
  shared_context_state->PessimisticallyResetGrContext();
  shared_context_state->set_need_context_state_reset(true);

  GetAblatedGpuBufferStore() = std::move(allocated_buffers);
  LOG(INFO) << "Applied GPU memory ablation: " << size_mb << " MB.";
  return GpuMemoryAblationResult::kSuccess;
}

void RunGpuMemoryAblation(GpuMemoryAblationAllocator allocator, int size_mb) {
  base::UmaHistogramEnumeration(kGpuResultHistogram,
                                std::move(allocator).Run(size_mb));
}

}  // namespace

void MaybeApplyMemoryAblation() {
  if (g_was_applied.exchange(true)) {
    return;
  }

  const bool is_enabled =
      base::FeatureList::IsEnabled(features::kCobaltNativeMemoryAblation);
  base::UmaHistogramBoolean("Cobalt.Features.NativeMemoryAblation.Enabled",
                            is_enabled);

  if (!is_enabled) {
    return;
  }

  const int size_mb = features::kMemoryAblationSizeMBParam.Get();
  base::UmaHistogramMemoryLargeMB(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", size_mb);

  if (size_mb <= 0) {
    return;
  }

  if (size_mb > kMaxAblationSizeMB) {
    base::UmaHistogramEnumeration("Cobalt.Features.NativeMemoryAblation.Result",
                                  NativeMemoryAblationResult::kExceedsMaxLimit);
    return;
  }

  const base::TimeDelta delay = features::kMemoryAblationDelayParam.Get();

  scoped_refptr<base::SequencedTaskRunner> reply_runner =
      base::SequencedTaskRunner::HasCurrentDefault()
          ? base::SequencedTaskRunner::GetCurrentDefault()
          : nullptr;

  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoMemoryAblationInBackground, size_mb, reply_runner),
      delay);
}

void MaybeApplyGpuMemoryAblation(gpu::GpuChannelManager* channel_manager) {
  base::WeakPtr<gpu::GpuChannelManager> weak_channel_manager =
      channel_manager ? channel_manager->AsWeakPtr() : nullptr;
  MaybeApplyGpuMemoryAblationWithAllocator(base::BindOnce(
      &AllocateGpuMemoryOnGpuThread, std::move(weak_channel_manager)));
}

void MaybeApplyGpuMemoryAblationWithAllocator(
    GpuMemoryAblationAllocator allocator) {
  if (g_gpu_was_applied.exchange(true)) {
    return;
  }

  const bool is_enabled =
      base::FeatureList::IsEnabled(features::kCobaltGpuMemoryAblation);
  base::UmaHistogramBoolean("Cobalt.Features.GpuMemoryAblation.Enabled",
                            is_enabled);

  if (!is_enabled) {
    return;
  }

  const int size_mb = features::kGpuMemoryAblationSizeMBParam.Get();
  base::UmaHistogramMemoryLargeMB(
      "Cobalt.Features.GpuMemoryAblation.AllocatedMB", size_mb);

  if (size_mb <= 0) {
    return;
  }

  if (size_mb > kMaxAblationSizeMB) {
    base::UmaHistogramEnumeration(kGpuResultHistogram,
                                  GpuMemoryAblationResult::kExceedsMaxLimit);
    return;
  }

  const base::TimeDelta delay = features::kGpuMemoryAblationDelayParam.Get();

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RunGpuMemoryAblation, std::move(allocator), size_mb),
        delay);
  } else {
    RunGpuMemoryAblation(std::move(allocator), size_mb);
  }
}

void ResetMemoryAblationForTesting() {
  g_was_applied.store(false);
  GetAblatedMemoryStore().clear();

  g_gpu_was_applied.store(false);
  GetAblatedGpuBufferStore().clear();
}

}  // namespace cobalt
