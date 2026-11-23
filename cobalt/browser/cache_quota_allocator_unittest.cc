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

#include <limits>
#include <ostream>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

void PrintTo(const CacheQuotaSizes& sizes, std::ostream* os) {
  *os << "{http_cache=" << sizes.http_cache
      << ", js_code_cache=" << sizes.js_code_cache
      << ", wasm_code_cache=" << sizes.wasm_code_cache
      << ", cache_storage=" << sizes.cache_storage
      << ", gpu_shader_cache=" << sizes.gpu_shader_cache
      << ", other=" << sizes.other << "}";
}

void PrintTo(CacheQuotaOutcome outcome, std::ostream* os) {
  *os << static_cast<int>(outcome);
}

namespace {

constexpr int64_t kMiB = 1024 * 1024;
constexpr int64_t kInt32Max = std::numeric_limits<int32_t>::max();
// The largest kSbMaxSystemPathCacheDirectorySize, 4 GiB - 1.
constexpr int64_t kUint32Max = std::numeric_limits<uint32_t>::max();

constexpr CacheQuotaPolicy kAllPolicies[] = {
    CacheQuotaPolicy::kHttp, CacheQuotaPolicy::kScale, CacheQuotaPolicy::kNone};

constexpr CacheQuotaSizes MiB(int64_t http_cache,
                              int64_t js_code_cache,
                              int64_t wasm_code_cache,
                              int64_t cache_storage,
                              int64_t gpu_shader_cache,
                              int64_t other) {
  return {.http_cache = http_cache * kMiB,
          .js_code_cache = js_code_cache * kMiB,
          .wasm_code_cache = wasm_code_cache * kMiB,
          .cache_storage = cache_storage * kMiB,
          .gpu_shader_cache = gpu_shader_cache * kMiB,
          .other = other * kMiB};
}

constexpr CacheQuotaSizes kDefaults = MiB(9, 5, 1, 6, 2, 1);

int64_t Sum(const CacheQuotaSizes& sizes) {
  return sizes.http_cache + sizes.js_code_cache + sizes.wasm_code_cache +
         sizes.cache_storage + sizes.gpu_shader_cache + sizes.other;
}

// Allocates with the GPU shader cache counted.
CacheQuotaAllocation Allocate(const CacheQuotaSizes& configured,
                              int64_t platform_budget_bytes,
                              CacheQuotaPolicy policy) {
  return AllocateCacheQuotas(configured, platform_budget_bytes,
                             /*gpu_shader_cache_counts=*/true, policy);
}

// Allocates with the GPU shader cache not counted.
CacheQuotaAllocation AllocateWithoutGpu(const CacheQuotaSizes& configured,
                                        int64_t platform_budget_bytes,
                                        CacheQuotaPolicy policy) {
  return AllocateCacheQuotas(configured, platform_budget_bytes,
                             /*gpu_shader_cache_counts=*/false, policy);
}

TEST(CacheQuotaAllocatorTest, DefaultsMatchThePlan) {
  EXPECT_EQ(kDefaultCacheQuotaSizes, kDefaults);
  EXPECT_EQ(kMinCacheQuotaBudgetBytes, 24 * kMiB);
  EXPECT_EQ(Sum(kDefaultCacheQuotaSizes), kMinCacheQuotaBudgetBytes);
  EXPECT_EQ(kMaxCacheQuotaBudgetBytes, kUint32Max);
}

// The outcome is recorded in a histogram, so its values must not change.
TEST(CacheQuotaAllocatorTest, OutcomeValuesAreStable) {
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kExact), 0);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kUnderCommittedResolvedByHttp),
            1);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kUnderCommittedResolvedByScale),
            2);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kUnderCommittedLeftByNone), 3);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kOverCommittedResolvedByHttp),
            4);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kOverCommittedResolvedByScale),
            5);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kOverCommittedLeftByNone), 6);
  EXPECT_EQ(
      static_cast<int>(CacheQuotaOutcome::kFellBackToDefaultsInvalidParams), 7);
  EXPECT_EQ(static_cast<int>(
                CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit),
            8);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kBudgetBelowMinimum), 9);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kDeveloperOverride), 10);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kFeatureDisabled), 11);
  EXPECT_EQ(static_cast<int>(CacheQuotaOutcome::kMaxValue), 11);
}

TEST(CacheQuotaAllocatorTest, DefaultsOnTheMinimumBudgetAreExact) {
  for (CacheQuotaPolicy policy : kAllPolicies) {
    SCOPED_TRACE(static_cast<int>(policy));
    const CacheQuotaAllocation allocation =
        Allocate(kDefaultCacheQuotaSizes, 24 * kMiB, policy);
    EXPECT_EQ(allocation.sizes, MiB(9, 5, 1, 6, 2, 1));
    EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kExact);
    EXPECT_FALSE(allocation.budget_below_minimum);
    EXPECT_EQ(allocation.budget_bytes, 24 * kMiB);
    EXPECT_EQ(allocation.uncommitted_bytes, 0);
  }
}

// Quotas below the defaults are legal as long as nothing has to shrink.
TEST(CacheQuotaAllocatorTest, ExactSumBelowTheDefaultsIsKept) {
  const CacheQuotaSizes configured = MiB(4, 10, 1, 6, 2, 1);
  for (CacheQuotaPolicy policy : kAllPolicies) {
    SCOPED_TRACE(static_cast<int>(policy));
    const CacheQuotaAllocation allocation =
        Allocate(configured, 24 * kMiB, policy);
    EXPECT_EQ(allocation.sizes, configured);
    EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kExact);
    EXPECT_EQ(allocation.uncommitted_bytes, 0);
  }
}

TEST(CacheQuotaAllocatorTest, UnderCommitHttpGivesTheExtraToHttp) {
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, 32 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(17, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_FALSE(allocation.budget_below_minimum);
  EXPECT_EQ(allocation.budget_bytes, 32 * kMiB);
  EXPECT_EQ(allocation.uncommitted_bytes, 8 * kMiB);
}

TEST(CacheQuotaAllocatorTest, UnderCommitScaleGrowsEveryQuota) {
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, 48 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, MiB(18, 10, 2, 12, 4, 2));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 24 * kMiB);
}

TEST(CacheQuotaAllocatorTest, UnderCommitNoneLeavesTheQuotas) {
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, 32 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kUnderCommittedLeftByNone);
  EXPECT_EQ(allocation.budget_bytes, 32 * kMiB);
  EXPECT_EQ(allocation.uncommitted_bytes, 8 * kMiB);
}

// The defaults are floors only when a quota has to shrink.
TEST(CacheQuotaAllocatorTest, UnderCommitAllowsQuotasBelowTheDefaults) {
  const CacheQuotaSizes configured = MiB(2, 2, 1, 1, 1, 1);

  CacheQuotaAllocation allocation =
      Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(18, 2, 1, 1, 1, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_EQ(allocation.uncommitted_bytes, 16 * kMiB);

  allocation = Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, MiB(6, 6, 3, 3, 3, 3));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 16 * kMiB);

  allocation = Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, configured);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kUnderCommittedLeftByNone);
  EXPECT_EQ(allocation.uncommitted_bytes, 16 * kMiB);
}

TEST(CacheQuotaAllocatorTest, OverCommitHttpTakesTheDeficitFromHttp) {
  // HTTP ends exactly on its floor, which is allowed.
  CacheQuotaAllocation allocation =
      Allocate(MiB(12, 5, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByHttp);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // Only HTTP shrinks, so a JS quota configured below its default is kept.
  allocation =
      Allocate(MiB(20, 4, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(10, 4, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByHttp);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, OverCommitHttpFallsBackWhenHttpWouldBreakFloor) {
  // One byte over the budget would leave HTTP one byte below its default.
  CacheQuotaSizes configured = kDefaults;
  configured.js_code_cache += 1;
  CacheQuotaAllocation allocation =
      Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // Raising only the JS quota to 10 MiB, as in the plan's risk section.
  allocation =
      Allocate(MiB(9, 10, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);

  // HTTP configured below its default cannot give up anything at all.
  allocation =
      Allocate(MiB(4, 12, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
}

TEST(CacheQuotaAllocatorTest, FallbackThenHttpHandsOutTheLeftoverBudget) {
  // 29 MiB on a 26 MiB budget would leave HTTP at 6 MiB. The defaults use 24
  // MiB, and HTTP then takes the 2 MiB left over.
  const CacheQuotaAllocation allocation =
      Allocate(MiB(9, 10, 1, 6, 2, 1), 26 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(11, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, OverCommitScaleShrinksEveryQuota) {
  // Every quota ends exactly on its floor, which is allowed.
  CacheQuotaAllocation allocation =
      Allocate(MiB(18, 10, 2, 12, 4, 2), 24 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // 48 MiB scaled by 3/4.
  allocation =
      Allocate(MiB(18, 10, 2, 12, 4, 2), 36 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 14155776,
                                               .js_code_cache = 7864320,
                                               .wasm_code_cache = 1572864,
                                               .cache_storage = 9437184,
                                               .gpu_shader_cache = 3145728,
                                               .other = 1572864}));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, OverCommitScaleFallsBackWhenAQuotaBreaksFloor) {
  // 33 MiB scaled to 24 MiB would shrink JS from 5 MiB to about 3.6 MiB.
  CacheQuotaAllocation allocation =
      Allocate(MiB(18, 5, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // Scaling shrinks every quota, so one configured below its default always
  // breaks its floor, however small the deficit.
  allocation =
      Allocate(MiB(20, 4, 2, 12, 4, 2), 43 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
}

TEST(CacheQuotaAllocatorTest, FallbackThenScaleHandsOutTheLeftoverBudget) {
  // 33 MiB scaled to 30 MiB would shrink JS to below 5 MiB. The defaults are
  // then scaled by 30/24.
  const CacheQuotaAllocation allocation =
      Allocate(MiB(18, 5, 1, 6, 2, 1), 30 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 11796480,
                                               .js_code_cache = 6553600,
                                               .wasm_code_cache = 1310720,
                                               .cache_storage = 7864320,
                                               .gpu_shader_cache = 2621440,
                                               .other = 1310720}));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, OverCommitNoneLeavesTheQuotasOverBudget) {
  const CacheQuotaSizes configured = MiB(20, 10, 1, 6, 2, 1);
  const CacheQuotaAllocation allocation =
      Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, configured);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kOverCommittedLeftByNone);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

// Policy "none" never shrinks anything, so it never falls back either.
TEST(CacheQuotaAllocatorTest, OverCommitNoneKeepsQuotasBelowTheDefaults) {
  const CacheQuotaSizes configured = MiB(30, 1, 1, 1, 1, 1);
  const CacheQuotaAllocation allocation =
      Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, configured);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kOverCommittedLeftByNone);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, AnyInvalidQuotaDiscardsAllOfThem) {
  constexpr int64_t CacheQuotaSizes::* kBuckets[] = {
      &CacheQuotaSizes::http_cache,       &CacheQuotaSizes::js_code_cache,
      &CacheQuotaSizes::wasm_code_cache,  &CacheQuotaSizes::cache_storage,
      &CacheQuotaSizes::gpu_shader_cache, &CacheQuotaSizes::other,
  };
  constexpr int64_t kInvalidSizes[] = {
      0,
      -1,
      -5 * kMiB,
      kInt32Max + 1,
      kUint32Max,
      std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::min(),
  };
  for (auto bucket : kBuckets) {
    for (int64_t invalid_size : kInvalidSizes) {
      for (CacheQuotaPolicy policy : kAllPolicies) {
        SCOPED_TRACE(testing::Message()
                     << "size " << invalid_size << ", policy "
                     << static_cast<int>(policy));
        // Valid quotas that differ from the defaults, to show that they are
        // discarded too.
        CacheQuotaSizes configured = MiB(8, 6, 1, 6, 2, 1);
        configured.*bucket = invalid_size;
        const CacheQuotaAllocation allocation =
            Allocate(configured, 24 * kMiB, policy);
        EXPECT_EQ(allocation.sizes, kDefaults);
        EXPECT_EQ(allocation.outcome,
                  CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
        EXPECT_FALSE(allocation.budget_below_minimum);
        EXPECT_EQ(allocation.uncommitted_bytes, 0);
      }
    }
  }
}

TEST(CacheQuotaAllocatorTest, LargestInt32QuotaIsValid) {
  CacheQuotaSizes configured = kDefaults;
  configured.http_cache = kInt32Max;

  CacheQuotaAllocation allocation =
      Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, configured);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kOverCommittedLeftByNone);

  // HTTP gives up everything above its default.
  allocation = Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByHttp);
}

TEST(CacheQuotaAllocatorTest, InvalidQuotasThenPolicyHandsOutLeftoverBudget) {
  CacheQuotaSizes configured = MiB(8, 6, 1, 6, 2, 1);
  configured.wasm_code_cache = 0;

  CacheQuotaAllocation allocation =
      Allocate(configured, 32 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(17, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_EQ(allocation.uncommitted_bytes, 8 * kMiB);

  allocation = Allocate(configured, 48 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, MiB(18, 10, 2, 12, 4, 2));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_EQ(allocation.uncommitted_bytes, 24 * kMiB);

  allocation = Allocate(configured, 32 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_EQ(allocation.uncommitted_bytes, 8 * kMiB);
}

TEST(CacheQuotaAllocatorTest, GpuNotCountedIsZeroAndLeavesTheSum) {
  // 22 MiB of counted defaults on 24 MiB: HTTP takes the GPU cache's 2 MiB.
  CacheQuotaAllocation allocation =
      AllocateWithoutGpu(kDefaults, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(11, 5, 1, 6, 0, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_EQ(allocation.uncommitted_bytes, 2 * kMiB);

  allocation =
      AllocateWithoutGpu(kDefaults, 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, MiB(9, 5, 1, 6, 0, 1));
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kUnderCommittedLeftByNone);
  EXPECT_EQ(allocation.uncommitted_bytes, 2 * kMiB);

  // The other five quotas add up to the budget, whatever the GPU quota says.
  for (CacheQuotaPolicy policy : kAllPolicies) {
    SCOPED_TRACE(static_cast<int>(policy));
    allocation =
        AllocateWithoutGpu(MiB(11, 5, 1, 6, 100, 1), 24 * kMiB, policy);
    EXPECT_EQ(allocation.sizes, MiB(11, 5, 1, 6, 0, 1));
    EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kExact);
    EXPECT_EQ(allocation.uncommitted_bytes, 0);
  }
}

TEST(CacheQuotaAllocatorTest, GpuNotCountedIsLeftOutOfScaling) {
  // 22 MiB scaled by 24/22, rounded down. `other` also gets the 2 bytes lost
  // to rounding.
  CacheQuotaAllocation allocation =
      AllocateWithoutGpu(kDefaults, 24 * kMiB, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 10295109,
                                               .js_code_cache = 5719505,
                                               .wasm_code_cache = 1143901,
                                               .cache_storage = 6863406,
                                               .gpu_shader_cache = 0,
                                               .other = 1143903}));
  EXPECT_EQ(Sum(allocation.sizes), 24 * kMiB);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 2 * kMiB);

  // 48 MiB of counted quotas halved. The GPU cache has no floor to break.
  allocation = AllocateWithoutGpu(MiB(22, 10, 2, 12, 7, 2), 24 * kMiB,
                                  CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, MiB(11, 5, 1, 6, 0, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, GpuNotCountedIsNotValidated) {
  for (int64_t gpu_size : {int64_t{0}, int64_t{-1}, kInt32Max + 1,
                           std::numeric_limits<int64_t>::max()}) {
    SCOPED_TRACE(gpu_size);
    CacheQuotaSizes configured = MiB(8, 6, 1, 6, 2, 1);
    configured.gpu_shader_cache = gpu_size;

    CacheQuotaAllocation allocation =
        AllocateWithoutGpu(configured, 24 * kMiB, CacheQuotaPolicy::kHttp);
    EXPECT_EQ(allocation.sizes, MiB(10, 6, 1, 6, 0, 1));
    EXPECT_EQ(allocation.outcome,
              CacheQuotaOutcome::kUnderCommittedResolvedByHttp);

    // The same quotas are rejected when the GPU cache counts.
    allocation = Allocate(configured, 24 * kMiB, CacheQuotaPolicy::kHttp);
    EXPECT_EQ(allocation.sizes, kDefaults);
    EXPECT_EQ(allocation.outcome,
              CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  }
}

TEST(CacheQuotaAllocatorTest, GpuNotCountedFallbacksUseDefaultsWithoutGpu) {
  // Invalid quotas.
  CacheQuotaAllocation allocation = AllocateWithoutGpu(
      MiB(9, 0, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, MiB(9, 5, 1, 6, 0, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_EQ(allocation.uncommitted_bytes, 2 * kMiB);

  // 27 MiB on 24 MiB would leave HTTP at 6 MiB. The defaults use 22 MiB, and
  // HTTP then takes the 2 MiB left over.
  allocation = AllocateWithoutGpu(MiB(9, 10, 1, 6, 2, 1), 24 * kMiB,
                                  CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(11, 5, 1, 6, 0, 1));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, BudgetBelowTheMinimumIsRaisedToIt) {
  for (int64_t platform_budget :
       {int64_t{0}, int64_t{-1}, std::numeric_limits<int64_t>::min(), 16 * kMiB,
        24 * kMiB - 1}) {
    for (CacheQuotaPolicy policy : kAllPolicies) {
      SCOPED_TRACE(testing::Message()
                   << "budget " << platform_budget << ", policy "
                   << static_cast<int>(policy));
      const CacheQuotaAllocation allocation =
          Allocate(kDefaults, platform_budget, policy);
      EXPECT_EQ(allocation.sizes, kDefaults);
      EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
      EXPECT_TRUE(allocation.budget_below_minimum);
      EXPECT_EQ(allocation.budget_bytes, 24 * kMiB);
      EXPECT_EQ(allocation.uncommitted_bytes, 0);
    }
  }
}

// The quotas are still fitted to the raised budget, but the outcome only says
// that the budget was too small.
TEST(CacheQuotaAllocatorTest, BudgetBelowTheMinimumOutranksOtherOutcomes) {
  // Invalid quotas.
  CacheQuotaAllocation allocation =
      Allocate(MiB(9, 0, 1, 6, 2, 1), 16 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);

  // An over-commit that cannot be resolved.
  allocation =
      Allocate(MiB(9, 10, 1, 6, 2, 1), 16 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);

  // An over-commit that HTTP resolves against 24 MiB, not against 16 MiB.
  allocation =
      Allocate(MiB(12, 5, 1, 6, 2, 1), 16 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // An over-commit that is left alone.
  allocation = Allocate(MiB(12, 5, 1, 6, 2, 1), 0, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, MiB(12, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // An under-commit against the raised budget.
  allocation = Allocate(MiB(2, 2, 1, 1, 1, 1), 0, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.sizes, MiB(18, 2, 1, 1, 1, 1));
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);
  EXPECT_EQ(allocation.uncommitted_bytes, 16 * kMiB);
}

TEST(CacheQuotaAllocatorTest, InvalidQuotasOutrankPolicyOutcomes) {
  // The defaults are under-committed on 32 MiB, and the outcome still reports
  // the invalid quota.
  const CacheQuotaAllocation allocation =
      Allocate(MiB(9, -1, 1, 6, 2, 1), 32 * kMiB, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_FALSE(allocation.budget_below_minimum);
}

// The outcome is a single value, so the allocation also says separately what
// the outcome may hide.
TEST(CacheQuotaAllocatorTest, ReportsWhatTheOutcomeMayHide) {
  CacheQuotaAllocation allocation =
      Allocate(kDefaults, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_FALSE(allocation.invalid_params);
  EXPECT_FALSE(allocation.unresolvable_over_commit);
  EXPECT_EQ(allocation.platform_budget_bytes, 24 * kMiB);
  EXPECT_EQ(allocation.configured_bytes, 24 * kMiB);

  // An over-commit that the policy resolves is not a fallback.
  allocation =
      Allocate(MiB(12, 5, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_FALSE(allocation.invalid_params);
  EXPECT_FALSE(allocation.unresolvable_over_commit);
  EXPECT_EQ(allocation.configured_bytes, 27 * kMiB);

  // Invalid quotas on a budget below the minimum. The configured sum is that
  // of the defaults, which replaced them.
  allocation =
      Allocate(MiB(12, 0, 1, 6, 2, 1), 16 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);
  EXPECT_TRUE(allocation.invalid_params);
  EXPECT_FALSE(allocation.unresolvable_over_commit);
  EXPECT_EQ(allocation.platform_budget_bytes, 16 * kMiB);
  EXPECT_EQ(allocation.budget_bytes, 24 * kMiB);
  EXPECT_EQ(allocation.configured_bytes, 24 * kMiB);

  // An unresolvable over-commit on a budget below the minimum. The configured
  // sum is that of the quotas that could not be fitted.
  allocation = Allocate(MiB(9, 10, 1, 6, 2, 1), 0, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
  EXPECT_TRUE(allocation.budget_below_minimum);
  EXPECT_FALSE(allocation.invalid_params);
  EXPECT_TRUE(allocation.unresolvable_over_commit);
  EXPECT_EQ(allocation.platform_budget_bytes, 0);
  EXPECT_EQ(allocation.configured_bytes, 29 * kMiB);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);

  // The same fallbacks on a large enough budget.
  allocation =
      Allocate(MiB(12, 0, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_TRUE(allocation.invalid_params);
  EXPECT_FALSE(allocation.unresolvable_over_commit);
  allocation =
      Allocate(MiB(9, 10, 1, 6, 2, 1), 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_FALSE(allocation.invalid_params);
  EXPECT_TRUE(allocation.unresolvable_over_commit);

  // A GPU shader cache that does not count is not in the configured sum.
  allocation =
      AllocateWithoutGpu(kDefaults, 24 * kMiB, CacheQuotaPolicy::kHttp);
  EXPECT_EQ(allocation.configured_bytes, 22 * kMiB);

  // The platform budget is reported as it was passed in.
  allocation = Allocate(kDefaults, kUint32Max + 1, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.platform_budget_bytes, kUint32Max + 1);
  EXPECT_EQ(allocation.budget_bytes, kUint32Max);
  allocation = Allocate(kDefaults, -1, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.platform_budget_bytes, -1);
  EXPECT_EQ(allocation.budget_bytes, 24 * kMiB);
}

TEST(CacheQuotaAllocatorTest, TotalAddsUpEveryQuota) {
  EXPECT_EQ(kDefaults.Total(), 24 * kMiB);
  EXPECT_EQ(MiB(1, 2, 3, 4, 5, 6).Total(), 21 * kMiB);
  EXPECT_EQ(CacheQuotaSizes().Total(), 0);
}

TEST(CacheQuotaAllocatorTest, ScaleGivesBytesLostToRoundingToOther) {
  // 24 MiB of defaults on 24 MiB + 7 bytes. The quotas get 2.6, 1.5, 0.3, 1.8,
  // 0.6 and 0.3 of the 7 bytes, rounded down to 2, 1, 0, 1, 0 and 0, and
  // `other` also gets the 3 bytes that are left.
  CacheQuotaAllocation allocation =
      Allocate(kDefaults, 24 * kMiB + 7, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 9 * kMiB + 2,
                                               .js_code_cache = 5 * kMiB + 1,
                                               .wasm_code_cache = 1 * kMiB,
                                               .cache_storage = 6 * kMiB + 1,
                                               .gpu_shader_cache = 2 * kMiB,
                                               .other = 1 * kMiB + 3}));
  EXPECT_EQ(Sum(allocation.sizes), 24 * kMiB + 7);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 7);

  // 56 MiB + 40 bytes on 30 MiB + 5 bytes. `other` scales to 1685217 and gets
  // the 3 bytes lost to rounding.
  allocation = Allocate({.http_cache = 20 * kMiB + 1,
                         .js_code_cache = 11 * kMiB + 3,
                         .wasm_code_cache = 3 * kMiB + 5,
                         .cache_storage = 14 * kMiB + 7,
                         .gpu_shader_cache = 5 * kMiB + 11,
                         .other = 3 * kMiB + 13},
                        30 * kMiB + 5, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 11234737,
                                               .js_code_cache = 6179106,
                                               .wasm_code_cache = 1685213,
                                               .cache_storage = 7864319,
                                               .gpu_shader_cache = 2808690,
                                               .other = 1685220}));
  EXPECT_EQ(Sum(allocation.sizes), 30 * kMiB + 5);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, ScaledQuotasAlwaysAddUpToTheBudget) {
  constexpr CacheQuotaSizes kConfigurations[] = {
      kDefaults,
      MiB(2, 2, 1, 1, 1, 1),
      {.http_cache = 9999991,
       .js_code_cache = 5000011,
       .wasm_code_cache = 1000003,
       .cache_storage = 6000011,
       .gpu_shader_cache = 2000003,
       .other = 1},
      {.http_cache = 1,
       .js_code_cache = 1,
       .wasm_code_cache = 1,
       .cache_storage = 1,
       .gpu_shader_cache = 1,
       .other = 1},
  };
  constexpr int64_t kBudgets[] = {
      24 * kMiB + 1, 25 * kMiB - 1, 31 * kMiB + 12345,
      64 * kMiB + 1, 1000000007,
  };
  for (const CacheQuotaSizes& configured : kConfigurations) {
    for (int64_t budget : kBudgets) {
      for (bool gpu_shader_cache_counts : {true, false}) {
        SCOPED_TRACE(testing::Message()
                     << testing::PrintToString(configured) << ", budget "
                     << budget << ", GPU counts " << gpu_shader_cache_counts);
        const int64_t counted_sum =
            Sum(configured) -
            (gpu_shader_cache_counts ? 0 : configured.gpu_shader_cache);
        const CacheQuotaAllocation allocation =
            AllocateCacheQuotas(configured, budget, gpu_shader_cache_counts,
                                CacheQuotaPolicy::kScale);
        EXPECT_EQ(Sum(allocation.sizes), budget);
        EXPECT_EQ(allocation.uncommitted_bytes, budget - counted_sum);
        EXPECT_EQ(allocation.outcome,
                  CacheQuotaOutcome::kUnderCommittedResolvedByScale);
        EXPECT_EQ(allocation.sizes.gpu_shader_cache == 0,
                  !gpu_shader_cache_counts);
      }
    }
  }
}

TEST(CacheQuotaAllocatorTest, LargestBudgetWithHttpSaturatesHttp) {
  // HTTP would get 4279238655 bytes, which does not fit in an int32_t.
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, kUint32Max, CacheQuotaPolicy::kHttp);
  CacheQuotaSizes expected = kDefaults;
  expected.http_cache = kInt32Max;
  EXPECT_EQ(allocation.sizes, expected);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_FALSE(allocation.budget_below_minimum);
  EXPECT_EQ(allocation.budget_bytes, kUint32Max);
  EXPECT_EQ(allocation.uncommitted_bytes, 4269801471);
}

TEST(CacheQuotaAllocatorTest, LargestBudgetWithScaleAddsUpToTheBudget) {
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, kUint32Max, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 1610612735,
                                               .js_code_cache = 894784853,
                                               .wasm_code_cache = 178956970,
                                               .cache_storage = 1073741823,
                                               .gpu_shader_cache = 357913941,
                                               .other = 178956973}));
  EXPECT_EQ(Sum(allocation.sizes), kUint32Max);
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 4269801471);
}

TEST(CacheQuotaAllocatorTest, LargestBudgetWithScaleSaturatesALargeQuota) {
  // HTTP would scale from 1 GiB to 4232961029 bytes.
  CacheQuotaSizes configured = kDefaults;
  configured.http_cache = 1024 * kMiB;
  const CacheQuotaAllocation allocation =
      Allocate(configured, kUint32Max, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = kInt32Max,
                                               .js_code_cache = 20668755,
                                               .wasm_code_cache = 4133751,
                                               .cache_storage = 24802506,
                                               .gpu_shader_cache = 8267502,
                                               .other = 4133752}));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 3205496831);
}

TEST(CacheQuotaAllocatorTest, LargestBudgetWithNoneLeavesTheQuotas) {
  const CacheQuotaAllocation allocation =
      Allocate(kDefaults, kUint32Max, CacheQuotaPolicy::kNone);
  EXPECT_EQ(allocation.sizes, kDefaults);
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kUnderCommittedLeftByNone);
  EXPECT_EQ(allocation.uncommitted_bytes, 4269801471);
}

TEST(CacheQuotaAllocatorTest, LargestQuotasOnTheLargestBudgetDoNotOverflow) {
  const CacheQuotaSizes configured = {.http_cache = kInt32Max,
                                      .js_code_cache = kInt32Max,
                                      .wasm_code_cache = kInt32Max,
                                      .cache_storage = kInt32Max,
                                      .gpu_shader_cache = kInt32Max,
                                      .other = kInt32Max};
  // Six quotas of 2 GiB - 1 scaled to 4 GiB - 1 would be 715827882 each, which
  // is below the defaults of none of them.
  const CacheQuotaAllocation allocation =
      Allocate(configured, kUint32Max, CacheQuotaPolicy::kScale);
  EXPECT_EQ(allocation.sizes, (CacheQuotaSizes{.http_cache = 715827882,
                                               .js_code_cache = 715827882,
                                               .wasm_code_cache = 715827882,
                                               .cache_storage = 715827882,
                                               .gpu_shader_cache = 715827882,
                                               .other = 715827885}));
  EXPECT_EQ(allocation.outcome,
            CacheQuotaOutcome::kOverCommittedResolvedByScale);
  EXPECT_EQ(allocation.uncommitted_bytes, 0);
}

TEST(CacheQuotaAllocatorTest, BudgetAboveTheMaximumIsLoweredToIt) {
  for (CacheQuotaPolicy policy : kAllPolicies) {
    SCOPED_TRACE(static_cast<int>(policy));
    const CacheQuotaAllocation expected =
        Allocate(kDefaults, kUint32Max, policy);
    for (int64_t platform_budget :
         {kUint32Max + 1, std::numeric_limits<int64_t>::max()}) {
      const CacheQuotaAllocation allocation =
          Allocate(kDefaults, platform_budget, policy);
      EXPECT_EQ(allocation.sizes, expected.sizes);
      EXPECT_EQ(allocation.outcome, expected.outcome);
      EXPECT_FALSE(allocation.budget_below_minimum);
      EXPECT_EQ(allocation.budget_bytes, kUint32Max);
      EXPECT_EQ(allocation.uncommitted_bytes, expected.uncommitted_bytes);
    }
  }
}

TEST(CacheQuotaFeatureParamsTest, ParamNamesAndDefaultsMatchThePlan) {
  EXPECT_STREQ(features::kCobaltCacheQuotas.name, "CobaltCacheQuotas");
  EXPECT_EQ(features::kCobaltCacheQuotas.default_state,
            base::FEATURE_DISABLED_BY_DEFAULT);

  EXPECT_STREQ(features::kCobaltCacheQuotasHttpCacheSizeBytes.name,
               "CobaltCacheQuotas_http_cache_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasHttpCacheSizeBytes.default_value,
            9437184);
  EXPECT_STREQ(features::kCobaltCacheQuotasJsCodeCacheSizeBytes.name,
               "CobaltCacheQuotas_js_code_cache_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasJsCodeCacheSizeBytes.default_value,
            5242880);
  EXPECT_STREQ(features::kCobaltCacheQuotasWasmCodeCacheSizeBytes.name,
               "CobaltCacheQuotas_wasm_code_cache_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasWasmCodeCacheSizeBytes.default_value,
            1048576);
  EXPECT_STREQ(features::kCobaltCacheQuotasCacheStorageSizeBytes.name,
               "CobaltCacheQuotas_cache_storage_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasCacheStorageSizeBytes.default_value,
            6291456);
  EXPECT_STREQ(features::kCobaltCacheQuotasGpuShaderCacheSizeBytes.name,
               "CobaltCacheQuotas_gpu_shader_cache_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasGpuShaderCacheSizeBytes.default_value,
            2097152);
  EXPECT_STREQ(features::kCobaltCacheQuotasOtherSizeBytes.name,
               "CobaltCacheQuotas_other_size_bytes");
  EXPECT_EQ(features::kCobaltCacheQuotasOtherSizeBytes.default_value, 1048576);
  EXPECT_STREQ(features::kCobaltCacheQuotasUncommittedPolicy.name,
               "CobaltCacheQuotas_uncommitted_policy");
  EXPECT_EQ(
      std::string(features::kCobaltCacheQuotasUncommittedPolicy.default_value),
      "http");
}

TEST(CacheQuotaFeatureParamsTest, DisabledFeatureGivesTheDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kCobaltCacheQuotas);

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  EXPECT_EQ(config.sizes, kDefaults);
  EXPECT_EQ(config.policy, CacheQuotaPolicy::kHttp);
}

TEST(CacheQuotaFeatureParamsTest, EnabledFeatureWithoutParamsGivesTheDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCobaltCacheQuotas);

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  EXPECT_EQ(config.sizes, kDefaults);
  EXPECT_EQ(config.policy, CacheQuotaPolicy::kHttp);

  // The default params on the smallest budget need no adjustment.
  const CacheQuotaAllocation allocation =
      Allocate(config.sizes, 24 * kMiB, config.policy);
  EXPECT_EQ(allocation.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(allocation.outcome, CacheQuotaOutcome::kExact);
}

TEST(CacheQuotaFeatureParamsTest, ReadsEveryParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_http_cache_size_bytes", "4194304"},
       {"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"},
       {"CobaltCacheQuotas_wasm_code_cache_size_bytes", "2097152"},
       {"CobaltCacheQuotas_cache_storage_size_bytes", "3145728"},
       {"CobaltCacheQuotas_gpu_shader_cache_size_bytes", "1048576"},
       {"CobaltCacheQuotas_other_size_bytes", "3145728"},
       {"CobaltCacheQuotas_uncommitted_policy", "scale"}});

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  EXPECT_EQ(config.sizes, MiB(4, 10, 2, 3, 1, 3));
  EXPECT_EQ(config.policy, CacheQuotaPolicy::kScale);
}

TEST(CacheQuotaFeatureParamsTest, ParamsThatAreNotSetKeepTheirDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_http_cache_size_bytes", "4194304"},
       {"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"}});

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  EXPECT_EQ(config.sizes, MiB(4, 10, 1, 6, 2, 1));
  EXPECT_EQ(config.policy, CacheQuotaPolicy::kHttp);
}

TEST(CacheQuotaFeatureParamsTest, ReadsEveryPolicy) {
  const struct {
    const char* param;
    CacheQuotaPolicy policy;
  } kCases[] = {
      {"http", CacheQuotaPolicy::kHttp},
      {"scale", CacheQuotaPolicy::kScale},
      {"none", CacheQuotaPolicy::kNone},
      // Unknown policies are read as the default.
      {"HTTP", CacheQuotaPolicy::kHttp},
      {"largest", CacheQuotaPolicy::kHttp},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.param);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCacheQuotas,
        {{"CobaltCacheQuotas_uncommitted_policy", test_case.param}});

    const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
    EXPECT_EQ(config.sizes, kDefaults);
    EXPECT_EQ(config.policy, test_case.policy);
  }
}

// A size that base::FeatureParam<int> would silently replace with its default
// must reach the allocator as an invalid quota instead.
TEST(CacheQuotaFeatureParamsTest, SizesThatDoNotFitAreRejectedByTheAllocator) {
  const struct {
    const char* param;
    // Whether the size is passed on unchanged, rather than as a negative
    // number.
    bool is_passed_on;
    int64_t expected_size;
  } kCases[] = {
      {"0", true, 0},
      {"-1", true, -1},
      {"2147483648", true, kInt32Max + 1},
      {"4294967296", true, kUint32Max + 1},
      {"9223372036854775808", false, 0},
      {"99999999999999999999999999", false, 0},
      {"5MiB", false, 0},
      {"5242880.5", false, 0},
      {"abc", false, 0},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.param);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltCacheQuotas,
        {{"CobaltCacheQuotas_http_cache_size_bytes", "8388608"},
         {"CobaltCacheQuotas_js_code_cache_size_bytes", test_case.param}});

    const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
    EXPECT_EQ(config.sizes.http_cache, 8 * kMiB);
    if (test_case.is_passed_on) {
      EXPECT_EQ(config.sizes.js_code_cache, test_case.expected_size);
    } else {
      EXPECT_LT(config.sizes.js_code_cache, 0);
    }

    const CacheQuotaAllocation allocation =
        Allocate(config.sizes, 24 * kMiB, config.policy);
    EXPECT_EQ(allocation.sizes, kDefaults);
    EXPECT_EQ(allocation.outcome,
              CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  }
}

TEST(CacheQuotaFeatureParamsTest, LargestInt32SizeIsPassedOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_http_cache_size_bytes", "2147483647"},
       {"CobaltCacheQuotas_uncommitted_policy", "none"}});

  const CacheQuotaConfig config = GetCacheQuotaConfigFromFeatureParams();
  EXPECT_EQ(config.sizes.http_cache, kInt32Max);
  EXPECT_EQ(config.policy, CacheQuotaPolicy::kNone);
  EXPECT_EQ(Allocate(config.sizes, 24 * kMiB, config.policy).outcome,
            CacheQuotaOutcome::kOverCommittedLeftByNone);
}

}  // namespace
}  // namespace cobalt
