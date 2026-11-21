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

#ifndef COBALT_BROWSER_CACHE_QUOTAS_H_
#define COBALT_BROWSER_CACHE_QUOTAS_H_

#include <cstdint>
#include <optional>

#include "base/command_line.h"
#include "cobalt/browser/cache_quota_allocator.h"

namespace cobalt {

// Whether features::kCobaltCacheQuotas decides the size of every cache in the
// cache directory. That is only so on Starboard: the feature has no effect on
// other platforms. When this is false the caches choose their own sizes, as
// they did before the feature existed.
bool AreCacheQuotasEnabled();

// Allocates the cache quotas from the params of features::kCobaltCacheQuotas
// and the switches in `command_line`, for a cache directory budget of
// `platform_budget_bytes`. It logs a warning if the quotas are below the
// budget, and an error if they are above it, if a param is invalid, or if the
// budget is below kMinCacheQuotaBudgetBytes.
//
// --disable-preload-gpu-shader-disk-cache leaves the GPU shader cache out of
// the allocation. Otherwise --gpu-disk-cache-size-kb replaces the GPU shader
// cache quota after the allocation with GetGpuDiskCacheSizeSwitch(), as
// --max-http-cache-size does for the HTTP cache quota. A replaced quota is the
// size that its cache really has, so the quotas may then no longer fit the
// budget, and the outcome is CacheQuotaOutcome::kDeveloperOverride. Either
// switch is ignored, with a warning for --gpu-disk-cache-size-kb, if its value
// is not a number.
//
// Unless --gpu-disk-cache-size-kb replaced it, the GPU shader cache quota is
// the size that the GPU process gives its disk cache for a
// --gpu-disk-cache-size-kb of GetGpuDiskCacheSizeKBForQuota(), which can be
// up to 2 KB less than was allocated.
CacheQuotaAllocation ComputeCacheQuotas(const base::CommandLine& command_line,
                                        int64_t platform_budget_bytes);

// Returns the cache quotas of this process: ComputeCacheQuotas() for the
// command line of the process and kSbMaxSystemPathCacheDirectorySize. They are
// computed on the first call and stay the same from then on, so every cache
// sees the same allocation. Can be called on any thread.
//
// Only meaningful if AreCacheQuotasEnabled().
CacheQuotaAllocation GetCacheQuotas();

// Makes the next call to GetCacheQuotas() compute the quotas again.
void ResetCacheQuotasForTesting();

// Returns the value of --max-http-cache-size in bytes, if `command_line` has
// that switch with a number that is not negative.
std::optional<int> GetMaxHttpCacheSizeSwitch(
    const base::CommandLine& command_line);

// Returns the size in bytes that the GPU process gives its disk cache for
// `command_line`, if `command_line` has --gpu-disk-cache-size-kb with a
// number: the value of the switch, or the default size of the GPU process if
// the value is 0, either of them doubled if
// features::kAggressiveShaderCacheLimits is enabled. Matches
// gpu::GetDefaultGpuDiskCacheSize() on platforms other than Android, where the
// GPU process ignores the switch.
//
// Returns nullopt if the value is not a number. The GPU process would use its
// default size for it, so whoever enables the GPU disk cache has to replace
// the value.
std::optional<int64_t> GetGpuDiskCacheSizeSwitch(
    const base::CommandLine& command_line);

// Returns the value in KB to give --gpu-disk-cache-size-kb so that the GPU
// process gives its disk cache `quota_bytes`, rounded down to what the switch
// can express, but at least 1. The GPU process doubles the value of the switch
// if features::kAggressiveShaderCacheLimits is enabled, which this makes up
// for.
int GetGpuDiskCacheSizeKBForQuota(int64_t quota_bytes);

}  // namespace cobalt

#endif  // COBALT_BROWSER_CACHE_QUOTAS_H_
