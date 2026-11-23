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

#include "cobalt/browser/cache_quota_allocator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/browser/features.h"

namespace cobalt {

namespace {

// The caches read their quota as an int, so no quota may exceed this.
constexpr int64_t kMaxQuotaBytes = std::numeric_limits<int32_t>::max();

// What GetCacheQuotaConfigFromFeatureParams() returns for a size it cannot
// parse. Any value that is not positive makes AllocateCacheQuotas() reject the
// configuration.
constexpr int64_t kUnparsableSize = -1;

enum Bucket : size_t {
  kHttpCache,
  kJsCodeCache,
  kWasmCodeCache,
  kCacheStorage,
  kGpuShaderCache,
  kOther,
  kBucketCount,
};

using BucketSizes = std::array<int64_t, kBucketCount>;

constexpr int64_t Sum(const BucketSizes& sizes) {
  return std::accumulate(sizes.begin(), sizes.end(), int64_t{0});
}

// A GPU shader cache that does not count is carried as 0, which keeps it out
// of every sum and leaves it at 0 when the sizes are scaled.
constexpr BucketSizes ToBucketSizes(const CacheQuotaSizes& sizes,
                                    bool gpu_shader_cache_counts) {
  return {sizes.http_cache,
          sizes.js_code_cache,
          sizes.wasm_code_cache,
          sizes.cache_storage,
          gpu_shader_cache_counts ? sizes.gpu_shader_cache : 0,
          sizes.other};
}

CacheQuotaSizes ToCacheQuotaSizes(const BucketSizes& sizes) {
  return {.http_cache = sizes[kHttpCache],
          .js_code_cache = sizes[kJsCodeCache],
          .wasm_code_cache = sizes[kWasmCodeCache],
          .cache_storage = sizes[kCacheStorage],
          .gpu_shader_cache = sizes[kGpuShaderCache],
          .other = sizes[kOther]};
}

// The defaults fit in the smallest budget, so falling back to them never
// leaves the quotas over-committed.
static_assert(Sum(ToBucketSizes(kDefaultCacheQuotaSizes,
                                /*gpu_shader_cache_counts=*/true)) ==
              kMinCacheQuotaBudgetBytes);

// The largest quota times the largest budget must not overflow when scaling.
static_assert(kMaxQuotaBytes <=
              std::numeric_limits<int64_t>::max() / kMaxCacheQuotaBudgetBytes);

bool AreValid(const BucketSizes& sizes, bool gpu_shader_cache_counts) {
  for (size_t i = 0; i < kBucketCount; ++i) {
    if (i == kGpuShaderCache && !gpu_shader_cache_counts) {
      continue;
    }
    if (sizes[i] <= 0 || sizes[i] > kMaxQuotaBytes) {
      return false;
    }
  }
  return true;
}

// Fits `sizes` to `budget` as `policy` says, and returns what it did. Returns
// nullopt, leaving `sizes` alone, if `sizes` exceed `budget` and fitting them
// would shrink one to below its entry in `floors`.
std::optional<CacheQuotaOutcome> ApplyPolicy(CacheQuotaPolicy policy,
                                             int64_t budget,
                                             const BucketSizes& floors,
                                             BucketSizes& sizes) {
  const int64_t sum = Sum(sizes);
  if (sum == budget) {
    return CacheQuotaOutcome::kExact;
  }
  const bool over_committed = sum > budget;

  switch (policy) {
    case CacheQuotaPolicy::kNone:
      return over_committed ? CacheQuotaOutcome::kOverCommittedLeftByNone
                            : CacheQuotaOutcome::kUnderCommittedLeftByNone;

    case CacheQuotaPolicy::kHttp: {
      const int64_t http_cache = sizes[kHttpCache] + (budget - sum);
      if (over_committed && http_cache < floors[kHttpCache]) {
        return std::nullopt;
      }
      sizes[kHttpCache] = http_cache;
      return over_committed ? CacheQuotaOutcome::kOverCommittedResolvedByHttp
                            : CacheQuotaOutcome::kUnderCommittedResolvedByHttp;
    }

    case CacheQuotaPolicy::kScale: {
      BucketSizes scaled;
      for (size_t i = 0; i < kBucketCount; ++i) {
        scaled[i] = sizes[i] * budget / sum;
      }
      // Rounding down loses less than one byte per bucket.
      scaled[kOther] += budget - Sum(scaled);
      if (over_committed) {
        for (size_t i = 0; i < kBucketCount; ++i) {
          if (scaled[i] < floors[i]) {
            return std::nullopt;
          }
        }
      }
      sizes = scaled;
      return over_committed ? CacheQuotaOutcome::kOverCommittedResolvedByScale
                            : CacheQuotaOutcome::kUnderCommittedResolvedByScale;
    }
  }
  NOTREACHED();
}

int64_t GetSizeParam(const base::FeatureParam<int>& param) {
  // base::FeatureParam<int>::Get() returns the default for a value that does
  // not fit in an int, which would hide a misconfiguration.
  const std::string value =
      base::GetFieldTrialParamValueByFeature(*param.feature, param.name);
  if (value.empty()) {
    return param.default_value;
  }
  int64_t size = 0;
  return base::StringToInt64(value, &size) ? size : kUnparsableSize;
}

CacheQuotaPolicy GetPolicyParam() {
  const std::string policy =
      features::kCobaltCacheQuotasUncommittedPolicy.Get();
  if (policy == "http") {
    return CacheQuotaPolicy::kHttp;
  }
  if (policy == "scale") {
    return CacheQuotaPolicy::kScale;
  }
  if (policy == "none") {
    return CacheQuotaPolicy::kNone;
  }
  LOG(WARNING) << "CobaltCacheQuotas: unknown "
               << features::kCobaltCacheQuotasUncommittedPolicy.name << " \""
               << policy << "\". Using \"http\" instead.";
  return CacheQuotaPolicy::kHttp;
}

}  // namespace

CacheQuotaAllocation AllocateCacheQuotas(const CacheQuotaSizes& configured,
                                         int64_t platform_budget_bytes,
                                         bool gpu_shader_cache_counts,
                                         CacheQuotaPolicy policy) {
  const int64_t budget =
      std::clamp(platform_budget_bytes, kMinCacheQuotaBudgetBytes,
                 kMaxCacheQuotaBudgetBytes);
  const BucketSizes defaults =
      ToBucketSizes(kDefaultCacheQuotaSizes, gpu_shader_cache_counts);

  BucketSizes sizes = ToBucketSizes(configured, gpu_shader_cache_counts);
  std::optional<CacheQuotaOutcome> fallback;
  if (!AreValid(sizes, gpu_shader_cache_counts)) {
    sizes = defaults;
    fallback = CacheQuotaOutcome::kFellBackToDefaultsInvalidParams;
  }

  const int64_t configured_bytes = Sum(sizes);

  std::optional<CacheQuotaOutcome> policy_outcome =
      ApplyPolicy(policy, budget, defaults, sizes);
  if (!policy_outcome) {
    sizes = defaults;
    fallback = CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit;
    policy_outcome = ApplyPolicy(policy, budget, defaults, sizes);
  }
  // The defaults are never over-committed, see the static_assert above.
  DCHECK(policy_outcome);

  for (int64_t& size : sizes) {
    size = std::min(size, kMaxQuotaBytes);
  }

  CacheQuotaAllocation allocation;
  allocation.sizes = ToCacheQuotaSizes(sizes);
  allocation.budget_below_minimum =
      platform_budget_bytes < kMinCacheQuotaBudgetBytes;
  allocation.invalid_params =
      fallback == CacheQuotaOutcome::kFellBackToDefaultsInvalidParams;
  allocation.unresolvable_over_commit =
      fallback == CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit;
  allocation.platform_budget_bytes = platform_budget_bytes;
  allocation.budget_bytes = budget;
  allocation.configured_bytes = configured_bytes;
  allocation.uncommitted_bytes =
      std::max(budget - configured_bytes, int64_t{0});
  if (allocation.budget_below_minimum) {
    allocation.outcome = CacheQuotaOutcome::kBudgetBelowMinimum;
  } else if (fallback) {
    allocation.outcome = *fallback;
  } else {
    allocation.outcome = *policy_outcome;
  }
  return allocation;
}

CacheQuotaConfig GetCacheQuotaConfigFromFeatureParams() {
  return {
      .sizes =
          {.http_cache =
               GetSizeParam(features::kCobaltCacheQuotasHttpCacheSizeBytes),
           .js_code_cache =
               GetSizeParam(features::kCobaltCacheQuotasJsCodeCacheSizeBytes),
           .wasm_code_cache =
               GetSizeParam(features::kCobaltCacheQuotasWasmCodeCacheSizeBytes),
           .cache_storage =
               GetSizeParam(features::kCobaltCacheQuotasCacheStorageSizeBytes),
           .gpu_shader_cache = GetSizeParam(
               features::kCobaltCacheQuotasGpuShaderCacheSizeBytes),
           .other = GetSizeParam(features::kCobaltCacheQuotasOtherSizeBytes)},
      .policy = GetPolicyParam(),
  };
}

}  // namespace cobalt
