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

#ifndef COBALT_BROWSER_CACHE_QUOTA_ALLOCATOR_H_
#define COBALT_BROWSER_CACHE_QUOTA_ALLOCATOR_H_

#include <cstdint>

namespace cobalt {

// The smallest cache directory budget a platform may report. A smaller budget
// is treated as this value.
inline constexpr int64_t kMinCacheQuotaBudgetBytes = 24 * 1024 * 1024;

// The largest budget the allocator works with, which is the largest value of
// kSbMaxSystemPathCacheDirectorySize (a uint32_t). A larger budget is treated
// as this value.
inline constexpr int64_t kMaxCacheQuotaBudgetBytes = 0xFFFFFFFF;

// How to resolve a difference between the platform budget and the sum of the
// configured quotas. It applies in both directions.
enum class CacheQuotaPolicy {
  // The HTTP cache takes the extra budget, or gives up the deficit.
  kHttp,
  // Every counted quota is multiplied by budget / sum.
  kScale,
  // The quotas are left as configured, even if they exceed the budget.
  kNone,
};

// What the allocator did. One value describes a whole allocation; when several
// apply, the one listed last here wins, except that the two fallbacks cannot
// both apply.
//
// The values will be recorded in a histogram, so they must not be renumbered
// or reused.
enum class CacheQuotaOutcome {
  // The quotas add up to the budget.
  kExact = 0,
  // The quotas were below the budget, and the policy handed out the extra.
  kUnderCommittedResolvedByHttp = 1,
  kUnderCommittedResolvedByScale = 2,
  // The quotas were below the budget and were left there.
  kUnderCommittedLeftByNone = 3,
  // The quotas were above the budget, and the policy took back the deficit.
  kOverCommittedResolvedByHttp = 4,
  kOverCommittedResolvedByScale = 5,
  // The quotas were above the budget and were left there.
  kOverCommittedLeftByNone = 6,
  // A configured quota was invalid, so the defaults were used instead.
  kFellBackToDefaultsInvalidParams = 7,
  // The policy could not take back the deficit without pushing a quota below
  // its default, so the defaults were used instead.
  kFellBackToDefaultsUnresolvableOverCommit = 8,
  // The platform budget was below kMinCacheQuotaBudgetBytes.
  kBudgetBelowMinimum = 9,
  // A command line switch replaced an allocated quota. AllocateCacheQuotas()
  // never returns this; it is for the code that applies the switches.
  kDeveloperOverride = 10,
  // features::kCobaltCacheQuotas is disabled, so the caches chose their own
  // sizes. AllocateCacheQuotas() never returns this either.
  kFeatureDisabled = 11,
  kMaxValue = kFeatureDisabled,
};

// One size in bytes for each cache that shares the cache directory.
struct CacheQuotaSizes {
  int64_t http_cache = 0;
  int64_t js_code_cache = 0;
  int64_t wasm_code_cache = 0;
  // The quota of one origin, for Service Worker scripts and CacheStorage.
  int64_t cache_storage = 0;
  int64_t gpu_shader_cache = 0;
  // Everything else in the cache directory.
  int64_t other = 0;

  friend bool operator==(const CacheQuotaSizes&,
                         const CacheQuotaSizes&) = default;
};

// The default quotas. They add up to kMinCacheQuotaBudgetBytes, and they are
// also the floors below which resolving an over-commit never shrinks a quota.
inline constexpr CacheQuotaSizes kDefaultCacheQuotaSizes = {
    .http_cache = 9 * 1024 * 1024,
    .js_code_cache = 5 * 1024 * 1024,
    .wasm_code_cache = 1 * 1024 * 1024,
    .cache_storage = 6 * 1024 * 1024,
    .gpu_shader_cache = 2 * 1024 * 1024,
    .other = 1 * 1024 * 1024,
};

struct CacheQuotaConfig {
  CacheQuotaSizes sizes = kDefaultCacheQuotaSizes;
  CacheQuotaPolicy policy = CacheQuotaPolicy::kHttp;
};

struct CacheQuotaAllocation {
  // The quotas to enforce. Each is at most the largest int32_t.
  CacheQuotaSizes sizes;
  CacheQuotaOutcome outcome = CacheQuotaOutcome::kExact;
  // Whether the platform budget was below kMinCacheQuotaBudgetBytes. `outcome`
  // also says so, but this stays readable if `outcome` is later replaced by
  // kDeveloperOverride.
  bool budget_below_minimum = false;
  // The budget the quotas were fitted to: the platform budget, limited to
  // kMinCacheQuotaBudgetBytes..kMaxCacheQuotaBudgetBytes.
  int64_t budget_bytes = 0;
  // How far the quotas were below `budget_bytes` before the policy was
  // applied: `budget_bytes` minus the sum of the configured quotas, or of the
  // defaults if a configured quota was invalid. It is 0 if they were above the
  // budget.
  int64_t uncommitted_bytes = 0;
};

// Fits the `configured` quotas to the platform's cache directory budget. This
// is a pure function: it reads no feature, switch or global.
//
// 1. If a quota is not positive or does not fit in an int32_t, all of
//    `configured` is discarded and the defaults are used.
// 2. The budget is `platform_budget_bytes`, raised to
//    kMinCacheQuotaBudgetBytes if it is below that.
// 3. If the quotas add up to the budget, they are returned as they are.
//    Otherwise `policy` decides. Quotas below the defaults are legal, but
//    `policy` may not shrink a quota to below its default to fit the budget:
//    if it would have to, all of `configured` is discarded, the defaults are
//    used, and `policy` hands out whatever budget is left over.
//
// CacheQuotaPolicy::kScale rounds down and gives the bytes lost to rounding to
// `other`, so that the quotas still add up to the budget.
//
// If `gpu_shader_cache_counts` is false the GPU shader cache is not in use:
// its quota is ignored, is not validated, takes no part in the sum, and is
// returned as 0.
CacheQuotaAllocation AllocateCacheQuotas(const CacheQuotaSizes& configured,
                                         int64_t platform_budget_bytes,
                                         bool gpu_shader_cache_counts,
                                         CacheQuotaPolicy policy);

// Reads the params of features::kCobaltCacheQuotas. Returns the defaults if
// the feature is disabled. A size that is not a number, or is too large for an
// int64_t, is returned as a negative number so that AllocateCacheQuotas()
// rejects it. An unknown policy is returned as CacheQuotaPolicy::kHttp.
CacheQuotaConfig GetCacheQuotaConfigFromFeatureParams();

}  // namespace cobalt

#endif  // COBALT_BROWSER_CACHE_QUOTA_ALLOCATOR_H_
