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

#include "cobalt/browser/cache_quotas.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "starboard/configuration_constants.h"

namespace cobalt {

namespace {

struct ProcessCacheQuotas {
  base::Lock lock;
  std::optional<CacheQuotaAllocation> quotas GUARDED_BY(lock);
};

ProcessCacheQuotas& GetProcessCacheQuotas() {
  static base::NoDestructor<ProcessCacheQuotas> process_cache_quotas;
  return *process_cache_quotas;
}

// How many bytes the GPU process gives its disk cache for each KB of
// --gpu-disk-cache-size-kb.
size_t GetGpuDiskCacheBytesPerSwitchKB() {
  return base::FeatureList::IsEnabled(::features::kAggressiveShaderCacheLimits)
             ? 2 * 1024
             : 1024;
}

// Returns the size in bytes that the GPU process gives its disk cache for a
// --gpu-disk-cache-size-kb of `size_kb`. This is what
// gpu::GetDefaultGpuDiskCacheSize() does with the switch on platforms other
// than Android, overflow included. That function cannot be called instead, as
// it only reads the command line of the process.
int64_t GetGpuDiskCacheSizeInEffect(size_t size_kb) {
  size_t size_bytes = size_kb * 1024;
  if (size_bytes == 0) {
    size_bytes = gpu::kDefaultMaxProgramCacheMemoryBytes;
  }
  size_bytes *= GetGpuDiskCacheBytesPerSwitchKB() / 1024;
  return base::saturated_cast<int64_t>(size_bytes);
}

const char* GetPolicyName(CacheQuotaPolicy policy) {
  switch (policy) {
    case CacheQuotaPolicy::kHttp:
      return "http";
    case CacheQuotaPolicy::kScale:
      return "scale";
    case CacheQuotaPolicy::kNone:
      return "none";
  }
  NOTREACHED();
}

std::string ToString(const CacheQuotaSizes& sizes) {
  std::ostringstream os;
  os << "HTTP cache " << sizes.http_cache << ", JS code cache "
     << sizes.js_code_cache << ", wasm code cache " << sizes.wasm_code_cache
     << ", CacheStorage " << sizes.cache_storage << ", GPU shader cache "
     << sizes.gpu_shader_cache << ", other " << sizes.other;
  return os.str();
}

// Logs what the allocator found wrong with `configured`.
void LogAllocationProblems(const CacheQuotaSizes& configured,
                           CacheQuotaPolicy policy,
                           const CacheQuotaAllocation& allocation) {
  if (allocation.budget_below_minimum) {
    LOG(ERROR) << "CobaltCacheQuotas: the platform's cache directory budget of "
               << allocation.platform_budget_bytes
               << " bytes is below the minimum of " << kMinCacheQuotaBudgetBytes
               << " bytes. Using the minimum instead.";
  }

  if (allocation.invalid_params) {
    LOG(ERROR) << "CobaltCacheQuotas: invalid params: every quota must be "
                  "positive and at most 2147483647, but they are (in bytes): "
               << ToString(configured) << ". Using the default quotas instead.";
  }

  const int64_t over_committed_bytes =
      allocation.configured_bytes - allocation.budget_bytes;
  if (over_committed_bytes > 0) {
    std::ostringstream resolution;
    if (allocation.unresolvable_over_commit) {
      resolution << "Policy \"" << GetPolicyName(policy)
                 << "\" cannot fit them without shrinking a cache to below "
                    "its default quota. Using the default quotas instead.";
    } else if (policy == CacheQuotaPolicy::kNone) {
      resolution << "Policy \"none\" leaves them over the budget.";
    } else {
      resolution << "Policy \"" << GetPolicyName(policy)
                 << "\" shrank them to fit.";
    }
    LOG(ERROR) << "CobaltCacheQuotas: over-committed: the quotas add up to "
               << allocation.configured_bytes << " bytes, which is "
               << over_committed_bytes << " bytes more than the budget of "
               << allocation.budget_bytes << " bytes. " << resolution.str();
  } else if (allocation.uncommitted_bytes > 0) {
    LOG(WARNING) << "CobaltCacheQuotas: under-committed: the quotas add up to "
                 << allocation.configured_bytes << " bytes, which is "
                 << allocation.uncommitted_bytes
                 << " bytes less than the budget of " << allocation.budget_bytes
                 << " bytes. Policy \"" << GetPolicyName(policy) << "\" "
                 << (policy == CacheQuotaPolicy::kNone
                         ? "leaves the rest unused."
                         : "hands out the rest.");
  }
}

}  // namespace

bool AreCacheQuotasEnabled() {
#if BUILDFLAG(IS_STARBOARD)
  return base::FeatureList::IsEnabled(features::kCobaltCacheQuotas);
#else
  // TODO: b/555871214 - Fit the cache sizes to a budget on Android too. For
  // now it keeps its own HTTP cache default, and nothing checks a budget there.
  return false;
#endif
}

CacheQuotaAllocation ComputeCacheQuotas(const base::CommandLine& command_line,
                                        int64_t platform_budget_bytes) {
  for (const base::Feature* feature :
       {&features::kCobaltHttpCacheQuota,
        &features::kCobaltCodeCacheOptimization}) {
    if (base::FeatureList::IsEnabled(*feature)) {
      LOG(WARNING) << "CobaltCacheQuotas: " << feature->name
                   << " is also enabled. Its size param is ignored.";
    }
  }

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  const bool gpu_shader_cache_counts =
      !command_line.HasSwitch(::switches::kDisablePreloadGpuShaderDiskCache);
  CacheQuotaAllocation allocation =
      AllocateCacheQuotas(config.sizes, platform_budget_bytes,
                          gpu_shader_cache_counts, config.policy);
  LogAllocationProblems(config.sizes, config.policy, allocation);

  // The switches replace an allocated quota with the size that its cache
  // really has, so that the quotas reported here are the ones in effect. The
  // quotas may then no longer fit the budget.
  if (gpu_shader_cache_counts) {
    if (const std::optional<int64_t> gpu_shader_cache_bytes =
            GetGpuDiskCacheSizeSwitch(command_line)) {
      LOG(WARNING) << "CobaltCacheQuotas: --" << ::switches::kGpuDiskCacheSizeKB
                   << " overrides the allocated GPU shader cache quota of "
                   << allocation.sizes.gpu_shader_cache << " bytes. The GPU "
                   << "shader cache uses " << *gpu_shader_cache_bytes
                   << " bytes. The budget is not enforced.";
      allocation.sizes.gpu_shader_cache = *gpu_shader_cache_bytes;
      allocation.outcome = CacheQuotaOutcome::kDeveloperOverride;
    } else {
      if (command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB)) {
        LOG(WARNING) << "CobaltCacheQuotas: --"
                     << ::switches::kGpuDiskCacheSizeKB << "=\""
                     << command_line.GetSwitchValueASCII(
                            ::switches::kGpuDiskCacheSizeKB)
                     << "\" is not a size and is ignored. The GPU shader "
                        "cache keeps its allocated quota.";
      }
      // Cobalt sets the size of the GPU shader cache with the same switch,
      // which cannot express every quota. Report the size that the quota
      // becomes.
      allocation.sizes.gpu_shader_cache = GetGpuDiskCacheSizeInEffect(
          GetGpuDiskCacheSizeKBForQuota(allocation.sizes.gpu_shader_cache));
    }
  }
  if (const std::optional<int> http_cache_bytes =
          GetMaxHttpCacheSizeSwitch(command_line)) {
    LOG(WARNING) << "CobaltCacheQuotas: --" << switches::kMaxHttpCacheSize
                 << " overrides the allocated HTTP cache quota of "
                 << allocation.sizes.http_cache << " bytes with "
                 << *http_cache_bytes << " bytes. The budget is not enforced.";
    allocation.sizes.http_cache = *http_cache_bytes;
    allocation.outcome = CacheQuotaOutcome::kDeveloperOverride;
  }

  LOG(INFO) << "CobaltCacheQuotas: quotas in bytes: "
            << ToString(allocation.sizes) << ". Budget "
            << allocation.budget_bytes << ", outcome "
            << static_cast<int>(allocation.outcome) << ".";
  return allocation;
}

CacheQuotaAllocation GetCacheQuotas() {
  ProcessCacheQuotas& process_cache_quotas = GetProcessCacheQuotas();
  base::AutoLock auto_lock(process_cache_quotas.lock);
  if (!process_cache_quotas.quotas) {
    process_cache_quotas.quotas =
        ComputeCacheQuotas(*base::CommandLine::ForCurrentProcess(),
                           kSbMaxSystemPathCacheDirectorySize);
  }
  return *process_cache_quotas.quotas;
}

void ResetCacheQuotasForTesting() {
  ProcessCacheQuotas& process_cache_quotas = GetProcessCacheQuotas();
  base::AutoLock auto_lock(process_cache_quotas.lock);
  process_cache_quotas.quotas.reset();
}

std::optional<int> GetMaxHttpCacheSizeSwitch(
    const base::CommandLine& command_line) {
  int size = 0;
  if (command_line.HasSwitch(switches::kMaxHttpCacheSize) &&
      base::StringToInt(
          command_line.GetSwitchValueASCII(switches::kMaxHttpCacheSize),
          &size) &&
      size >= 0) {
    return size;
  }
  return std::nullopt;
}

std::optional<int64_t> GetGpuDiskCacheSizeSwitch(
    const base::CommandLine& command_line) {
  size_t size_kb = 0;
  if (command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB) &&
      base::StringToSizeT(
          command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
          &size_kb)) {
    return GetGpuDiskCacheSizeInEffect(size_kb);
  }
  return std::nullopt;
}

int GetGpuDiskCacheSizeKBForQuota(int64_t quota_bytes) {
  // 0 would make the GPU process use its default size.
  return base::saturated_cast<int>(std::max(
      quota_bytes /
          base::checked_cast<int64_t>(GetGpuDiskCacheBytesPerSwitchKB()),
      int64_t{1}));
}

}  // namespace cobalt
