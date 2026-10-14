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

#ifndef COBALT_BROWSER_MEMORY_ABLATION_H_
#define COBALT_BROWSER_MEMORY_ABLATION_H_

#include "base/functional/callback.h"

namespace gpu {
class GpuChannelManager;
}  // namespace gpu

namespace cobalt {

// Maximum allowed native/GPU memory ablation size in Megabytes (256 MB) to
// prevent extreme memory allocations or integer overflow on
// resource-constrained devices.
constexpr int kMaxAblationSizeMB = 256;

// Outcome of native memory ablation allocation attempt.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NativeMemoryAblationResult {
  kSuccess = 0,
  kOomFailure = 1,
  kExceedsMaxLimit = 2,
  kMaxValue = kExceedsMaxLimit,
};

// Outcome of GPU memory ablation allocation attempt.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuMemoryAblationResult {
  kSuccess = 0,
  kGlOutOfMemory = 1,
  kExceedsMaxLimit = 2,
  kChannelManagerUnavailable = 3,
  kContextUnavailable = 4,
  kContextLost = 5,
  kMakeCurrentFailed = 6,
  kGlApiUnavailable = 7,
  kGlOtherError = 8,
  kMaxValue = kGlOtherError,
};

// Checks if the native memory ablation Finch feature is enabled and,
// if so, allocates and commits (dirties) the requested amount of native memory
// on a background thread after an optional delay to hold for the lifetime of
// the process.
// Strictly executes at most once per application lifetime.
void MaybeApplyMemoryAblation();

// Checks if the GPU memory ablation Finch feature is enabled and, if so,
// allocates and commits the requested amount of GPU memory on the GPU
// main thread after an optional delay to hold for the lifetime of the process.
// Strictly executes at most once per application lifetime.
void MaybeApplyGpuMemoryAblation(gpu::GpuChannelManager* channel_manager);

// Allocates and retains |size_mb| MB of GPU memory, returning the outcome.
using GpuMemoryAblationAllocator =
    base::OnceCallback<GpuMemoryAblationResult(int size_mb)>;

// Same as MaybeApplyGpuMemoryAblation(), but performs the allocation with the
// injected |allocator|. MaybeApplyGpuMemoryAblation() calls this with the real
// GL allocator; it is exposed so tests can inject a fake allocator without a
// hardware GL context.
void MaybeApplyGpuMemoryAblationWithAllocator(
    GpuMemoryAblationAllocator allocator);

// Resets internal state for unit testing.
void ResetMemoryAblationForTesting();

}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_ABLATION_H_
