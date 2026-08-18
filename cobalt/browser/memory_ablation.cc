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
#include <memory>
#include <new>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"

namespace cobalt {

namespace {

std::atomic_bool g_was_applied{false};

// Holds allocated buffers alive for the duration of the process.
std::vector<std::unique_ptr<char[]>>& GetAblatedMemoryStore() {
  static base::NoDestructor<std::vector<std::unique_ptr<char[]>>> store;
  return *store;
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
    LOG(ERROR) << "Failed to allocate " << size_mb
               << " MB for native memory ablation (OOM).";
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
    LOG(ERROR) << "Requested ablation size " << size_mb
               << " MB exceeds maximum allowable limit (" << kMaxAblationSizeMB
               << " MB). Ablation skipped.";
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

void ResetMemoryAblationForTesting() {
  g_was_applied.store(false);
  GetAblatedMemoryStore().clear();
}

}  // namespace cobalt
