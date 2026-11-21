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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "starboard/configuration_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

using ::testing::_;
using ::testing::AnyNumber;

constexpr int64_t kMiB = 1024 * 1024;

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

// Collects the messages that ComputeCacheQuotas() logs while it is alive.
class CacheQuotaLogs {
 public:
  CacheQuotaLogs() {
    EXPECT_CALL(log_, Log(_, _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](int severity, const char* file, int line,
                               size_t message_start, const std::string& str) {
          if (str.find("CobaltCacheQuotas") != std::string::npos) {
            messages_.emplace_back(severity, str.substr(message_start));
          }
          return false;
        });
    log_.StartCapturingLogs();
  }

  // Returns how many messages of `severity` contain `text`.
  int Count(int severity, std::string_view text = "") const {
    int count = 0;
    for (const auto& [message_severity, message] : messages_) {
      if (message_severity == severity &&
          message.find(text) != std::string::npos) {
        ++count;
      }
    }
    return count;
  }

 private:
  base::test::MockLog log_;
  std::vector<std::pair<int, std::string>> messages_;
};

class CacheQuotasTest : public testing::Test {
 protected:
  void SetUp() override { ResetCacheQuotasForTesting(); }
  void TearDown() override { ResetCacheQuotasForTesting(); }

  void EnableWithParams(const base::FieldTrialParams& params) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCobaltCacheQuotas, params);
  }

  base::test::ScopedFeatureList feature_list_;
  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};
};

TEST_F(CacheQuotasTest, DisabledByDefault) {
  EXPECT_FALSE(AreCacheQuotasEnabled());
}

TEST_F(CacheQuotasTest, EnabledOnStarboardOnly) {
  EnableWithParams({});
#if BUILDFLAG(IS_STARBOARD)
  EXPECT_TRUE(AreCacheQuotasEnabled());
#else
  EXPECT_FALSE(AreCacheQuotasEnabled());
#endif
}

TEST_F(CacheQuotasTest, DefaultParamsOnTheMinimumBudget) {
  EnableWithParams({});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kExact);
  EXPECT_EQ(quotas.platform_budget_bytes, 24 * kMiB);
  EXPECT_EQ(quotas.uncommitted_bytes, 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_INFO, "quotas in bytes"), 1);
}

TEST_F(CacheQuotasTest, UsesTheParams) {
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "4194304"},
                    {"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"},
                    {"CobaltCacheQuotas_uncommitted_policy", "scale"}});
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 48 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(8, 20, 2, 12, 4, 2));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kUnderCommittedResolvedByScale);
  EXPECT_EQ(quotas.uncommitted_bytes, 24 * kMiB);
}

TEST_F(CacheQuotasTest, UnderCommitLogsAWarning) {
  EnableWithParams({});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 32 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(17, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "under-committed"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

TEST_F(CacheQuotasTest, OverCommitLogsAnError) {
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "12582912"}});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kOverCommittedResolvedByHttp);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "over-committed"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
}

TEST_F(CacheQuotasTest, UnresolvableOverCommitLogsAnErrorAndUsesTheDefaults) {
  // The plan's example: raising only the JS quota to 10 MiB.
  EnableWithParams(
      {{"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"}});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsUnresolvableOverCommit);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "over-committed"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "default quotas"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
}

TEST_F(CacheQuotasTest, InvalidParamsLogAnErrorAndUseTheDefaults) {
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "8388608"},
                    {"CobaltCacheQuotas_js_code_cache_size_bytes", "0"}});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome,
            CacheQuotaOutcome::kFellBackToDefaultsInvalidParams);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "invalid params"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
}

TEST_F(CacheQuotasTest, BudgetBelowTheMinimumLogsAnError) {
  EnableWithParams({});
  for (int64_t platform_budget : {int64_t{0}, 16 * kMiB}) {
    SCOPED_TRACE(platform_budget);
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line_, platform_budget);
    EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
    EXPECT_EQ(quotas.platform_budget_bytes, platform_budget);
    EXPECT_EQ(quotas.budget_bytes, 24 * kMiB);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "below the minimum"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
  }
}

// The outcome only says that the budget was too small, but every problem is
// still logged.
TEST_F(CacheQuotasTest, BudgetBelowTheMinimumDoesNotHideOtherErrors) {
  EnableWithParams({{"CobaltCacheQuotas_js_code_cache_size_bytes", "-1"}});
  {
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line_, 16 * kMiB);
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "below the minimum"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "invalid params"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 2);
  }

  feature_list_.Reset();
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "12582912"}});
  {
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line_, 16 * kMiB);
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kBudgetBelowMinimum);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "below the minimum"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "over-committed"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 2);
  }
}

TEST_F(CacheQuotasTest, PolicyNoneLeavesTheQuotasAndStillLogs) {
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "12582912"},
                    {"CobaltCacheQuotas_uncommitted_policy", "none"}});
  {
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line_, 24 * kMiB);
    EXPECT_EQ(quotas.sizes, MiB(12, 5, 1, 6, 2, 1));
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kOverCommittedLeftByNone);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "over-committed"), 1);
  }
  {
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line_, 32 * kMiB);
    EXPECT_EQ(quotas.sizes, MiB(12, 5, 1, 6, 2, 1));
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kUnderCommittedLeftByNone);
    EXPECT_EQ(quotas.uncommitted_bytes, 5 * kMiB);
    EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "under-committed"), 1);
  }
}

TEST_F(CacheQuotasTest, DisablePreloadSwitchLeavesTheGpuShaderCacheOut) {
  EnableWithParams({});
  command_line_.AppendSwitch(::switches::kDisablePreloadGpuShaderDiskCache);
  // An explicit size has no effect on a cache that stays disabled.
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "8192");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(11, 5, 1, 6, 0, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kUnderCommittedResolvedByHttp);
  EXPECT_EQ(quotas.uncommitted_bytes, 2 * kMiB);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"), 0);
}

// Like --max-http-cache-size, the switch replaces the quota after the
// allocation, so that the reported quota is the size the cache really has.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchOverridesTheAllocation) {
  EnableWithParams(
      {{"CobaltCacheQuotas_gpu_shader_cache_size_bytes", "1048576"}});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "4096");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 32 * kMiB);
  // The params add up to 23 MiB, with 1 MiB for the GPU shader cache, so the
  // HTTP cache takes 9 MiB more. Only then does the GPU shader cache get the
  // 4 MiB of the switch.
  EXPECT_EQ(quotas.sizes, MiB(18, 5, 1, 6, 4, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_EQ(quotas.budget_bytes, 32 * kMiB);
  EXPECT_EQ(quotas.configured_bytes, 23 * kMiB);
  EXPECT_EQ(quotas.uncommitted_bytes, 9 * kMiB);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "under-committed"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_INFO, "GPU shader cache 4194304"), 1);
}

// The switch takes no part in the allocation, so it cannot over-commit the
// params and make them fall back to the defaults.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchDoesNotCauseAFallback) {
  EnableWithParams({});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "4096");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  // The other quotas stay, so the total is over the budget.
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 4, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_FALSE(quotas.unresolvable_over_commit);
  EXPECT_EQ(quotas.uncommitted_bytes, 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

// A fallback caused by the params is still reported in the logs, and the
// switch still replaces the GPU shader cache quota of the defaults.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchAfterAFallback) {
  EnableWithParams(
      {{"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"}});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "512");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, (CacheQuotaSizes{.http_cache = 9 * kMiB,
                                           .js_code_cache = 5 * kMiB,
                                           .wasm_code_cache = 1 * kMiB,
                                           .cache_storage = 6 * kMiB,
                                           .gpu_shader_cache = 512 * 1024,
                                           .other = 1 * kMiB}));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_TRUE(quotas.unresolvable_over_commit);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "over-committed"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"), 1);
}

TEST_F(CacheQuotasTest, BothSwitchesAreOneDeveloperOverride) {
  EnableWithParams({});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "4096");
  command_line_.AppendSwitchASCII(switches::kMaxHttpCacheSize, "16777216");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(16, 5, 1, 6, 4, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "max-http-cache-size"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 2);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

// Like --max-http-cache-size, a switch that is not a number is ignored: the
// GPU shader cache keeps its allocated quota, and it is not an override.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchThatIsNotANumberIsIgnored) {
  EnableWithParams({});
  for (const char* value :
       {"", "-1", "abc", "2048kb", "1.5", "99999999999999999999999"}) {
    SCOPED_TRACE(value);
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, value);
    EXPECT_EQ(GetGpuDiskCacheSizeSwitch(command_line), std::nullopt);
    CacheQuotaLogs logs;
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line, 24 * kMiB);
    EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kExact);
    EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "is not a size"), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "gpu-disk-cache-size-kb"),
              1);
    EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 1);
    EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
  }
}

// Like --max-http-cache-size=0, a switch of 0 hands the size to the cache's
// own default. It is an override, and that default is reported.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchOfZeroIsTheGpuDefault) {
  EnableWithParams({});
  const int64_t gpu_default_bytes = gpu::kDefaultMaxProgramCacheMemoryBytes;
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "0");
  EXPECT_EQ(GetGpuDiskCacheSizeSwitch(command_line_), gpu_default_bytes);
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes,
            (CacheQuotaSizes{.http_cache = 9 * kMiB,
                             .js_code_cache = 5 * kMiB,
                             .wasm_code_cache = 1 * kMiB,
                             .cache_storage = 6 * kMiB,
                             .gpu_shader_cache = gpu_default_bytes,
                             .other = 1 * kMiB}));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "overrides the allocated"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "is not a size"), 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchIsInKilobytes) {
  EXPECT_EQ(GetGpuDiskCacheSizeSwitch(command_line_), std::nullopt);
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "512");
  EXPECT_EQ(GetGpuDiskCacheSizeSwitch(command_line_), 512 * 1024);
}

// The switch is reported as it is, even if it is larger than any quota that
// the params may configure.
TEST_F(CacheQuotasTest, HugeGpuDiskCacheSizeSwitchIsReportedAsItIs) {
  EnableWithParams({});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "2097152");
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, (CacheQuotaSizes{.http_cache = 9 * kMiB,
                                           .js_code_cache = 5 * kMiB,
                                           .wasm_code_cache = 1 * kMiB,
                                           .cache_storage = 6 * kMiB,
                                           .gpu_shader_cache = 2048 * kMiB,
                                           .other = 1 * kMiB}));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_FALSE(quotas.invalid_params);
}

// The GPU process doubles the size of its disk cache if
// kAggressiveShaderCacheLimits is enabled, so an explicit switch then stands
// for twice its value.
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchIsDoubledByAggressiveLimits) {
  feature_list_.InitWithFeatures(
      {features::kCobaltCacheQuotas, ::features::kAggressiveShaderCacheLimits},
      {});
  command_line_.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "4096");
  EXPECT_EQ(GetGpuDiskCacheSizeSwitch(command_line_), 8 * kMiB);
  EXPECT_EQ(ComputeCacheQuotas(command_line_, 24 * kMiB).sizes,
            MiB(9, 5, 1, 6, 8, 1));

  base::CommandLine zero(base::CommandLine::NO_PROGRAM);
  zero.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, "0");
  EXPECT_EQ(GetGpuDiskCacheSizeSwitch(zero),
            int64_t{2 * gpu::kDefaultMaxProgramCacheMemoryBytes});
}

// When Cobalt sets the size itself, it halves the value of the switch if the
// GPU process is going to double it, so that the GPU shader cache gets its
// quota and the quotas still fit the budget.
TEST_F(CacheQuotasTest, AggressiveLimitsDoNotChangeTheGpuShaderCacheQuota) {
  feature_list_.InitWithFeatures(
      {features::kCobaltCacheQuotas, ::features::kAggressiveShaderCacheLimits},
      {});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kExact);
  EXPECT_EQ(GetGpuDiskCacheSizeKBForQuota(quotas.sizes.gpu_shader_cache), 1024);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 0);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

struct GpuQuotaCase {
  // The GPU shader cache param.
  const char* param_bytes;
  // The value that Cobalt gives --gpu-disk-cache-size-kb for it.
  int switch_kb;
  // The size that the GPU process gives its disk cache for that value, which
  // is the quota that is reported.
  int64_t bytes_in_effect;
};

class CacheQuotasGpuSizeTest : public CacheQuotasTest {
 protected:
  // Enables the cache quotas with a GPU shader cache param of `param_bytes`
  // that the policy leaves alone.
  void EnableWithGpuParam(const char* param_bytes, bool aggressive_limits) {
    feature_list_.Reset();
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kCobaltCacheQuotas,
         {{"CobaltCacheQuotas_gpu_shader_cache_size_bytes", param_bytes},
          {"CobaltCacheQuotas_uncommitted_policy", "none"}}}};
    std::vector<base::test::FeatureRef> disabled;
    if (aggressive_limits) {
      enabled.push_back({::features::kAggressiveShaderCacheLimits, {}});
    } else {
      disabled.push_back(::features::kAggressiveShaderCacheLimits);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  // The size that the GPU process gives its disk cache for a
  // --gpu-disk-cache-size-kb of `switch_kb`.
  static int64_t GpuProcessDiskCacheSize(int switch_kb) {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kGpuDiskCacheSizeKB, base::NumberToString(switch_kb));
    return base::saturated_cast<int64_t>(gpu::GetDefaultGpuDiskCacheSize());
  }

  void ExpectCases(base::span<const GpuQuotaCase> cases,
                   bool aggressive_limits) {
    for (const GpuQuotaCase& test_case : cases) {
      SCOPED_TRACE(testing::Message()
                   << test_case.param_bytes << " bytes, aggressive "
                   << aggressive_limits);
      EnableWithGpuParam(test_case.param_bytes, aggressive_limits);
      const CacheQuotaAllocation quotas =
          ComputeCacheQuotas(command_line_, 24 * kMiB);
      EXPECT_EQ(quotas.sizes.gpu_shader_cache, test_case.bytes_in_effect);
      EXPECT_NE(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
      // The reported quota is one that the switch can express exactly.
      EXPECT_EQ(GetGpuDiskCacheSizeKBForQuota(quotas.sizes.gpu_shader_cache),
                test_case.switch_kb);
      // The other quotas are not affected.
      EXPECT_EQ(quotas.sizes.http_cache, 9 * kMiB);
      EXPECT_EQ(quotas.sizes.other, 1 * kMiB);
#if !BUILDFLAG(IS_ANDROID)
      // The GPU process ignores the switch on Android, where the cache quotas
      // are never enabled.
      EXPECT_EQ(GpuProcessDiskCacheSize(test_case.switch_kb),
                test_case.bytes_in_effect);
#endif  // !BUILDFLAG(IS_ANDROID)
    }
  }
};

TEST_F(CacheQuotasGpuSizeTest, QuotaIsTheSizeThatTheSwitchCanExpress) {
  static constexpr GpuQuotaCase kCases[] = {
      {"2097152", 2048, 2097152},
      {"4194304", 4096, 4194304},
      {"3072", 3, 3072},
      // Rounded down to whole KB.
      {"5000", 4, 4096},
      {"1500", 1, 1024},
      // 0 KB would mean the default size of the GPU process, so it is 1 KB.
      {"1000", 1, 1024},
      {"1", 1, 1024},
  };
  ExpectCases(kCases, /*aggressive_limits=*/false);
}

TEST_F(CacheQuotasGpuSizeTest, QuotaWithAggressiveLimitsIsAMultipleOfTwoKB) {
  static constexpr GpuQuotaCase kCases[] = {
      // Half the quota in the switch, all of it in effect.
      {"2097152", 1024, 2097152},
      {"4194304", 2048, 4194304},
      {"3145728", 1536, 3145728},
      // 2049 KB cannot be halved in whole KB, so it loses 1 KB.
      {"2098176", 1024, 2097152},
      {"5000", 2, 4096},
      // 3 KB becomes 2 KB.
      {"3072", 1, 2048},
      // The smallest size is 1 KB in the switch, which is 2 KB in effect.
      {"1500", 1, 2048},
      {"1000", 1, 2048},
  };
  ExpectCases(kCases, /*aggressive_limits=*/true);
}

// The reported size must be the one that the GPU process computes for the
// same command line, whatever the switch says. The GPU process ignores the
// switch on Android, where the cache quotas are never enabled.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(CacheQuotasTest, GpuDiskCacheSizeSwitchMatchesTheGpuProcess) {
  for (bool aggressive_limits : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(::features::kAggressiveShaderCacheLimits,
                                      aggressive_limits);
    for (const char* value : {"1", "512", "2048", "4096", "2097152", "0",
                              "18014398509481984", "18446744073709551615"}) {
      SCOPED_TRACE(testing::Message()
                   << "\"" << value << "\", aggressive " << aggressive_limits);
      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, value);
      EXPECT_EQ(
          GetGpuDiskCacheSizeSwitch(*command_line),
          base::saturated_cast<int64_t>(gpu::GetDefaultGpuDiskCacheSize()));
    }
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(CacheQuotasTest, MaxHttpCacheSizeSwitchOverridesTheAllocation) {
  EnableWithParams({});
  command_line_.AppendSwitchASCII(switches::kMaxHttpCacheSize, "16777216");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  // The other quotas stay, so the total is over the budget.
  EXPECT_EQ(quotas.sizes, MiB(16, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_EQ(quotas.budget_bytes, 24 * kMiB);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "max-http-cache-size"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR), 0);
}

TEST_F(CacheQuotasTest, MaxHttpCacheSizeSwitchOutranksEveryOtherOutcome) {
  EnableWithParams({{"CobaltCacheQuotas_js_code_cache_size_bytes", "0"}});
  command_line_.AppendSwitchASCII(switches::kMaxHttpCacheSize, "0");
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas = ComputeCacheQuotas(command_line_, 0);
  EXPECT_EQ(quotas.sizes, MiB(0, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_TRUE(quotas.budget_below_minimum);
  EXPECT_TRUE(quotas.invalid_params);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "below the minimum"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_ERROR, "invalid params"), 1);
}

TEST_F(CacheQuotasTest, MaxHttpCacheSizeSwitchThatIsNotASize) {
  EnableWithParams({});
  for (const char* value : {"", "-1", "abc", "16MiB", "4294967296"}) {
    SCOPED_TRACE(value);
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kMaxHttpCacheSize, value);
    EXPECT_EQ(GetMaxHttpCacheSizeSwitch(command_line), std::nullopt);
    const CacheQuotaAllocation quotas =
        ComputeCacheQuotas(command_line, 24 * kMiB);
    EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
    EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kExact);
  }
}

TEST_F(CacheQuotasTest, MaxHttpCacheSizeSwitchValue) {
  EXPECT_EQ(GetMaxHttpCacheSizeSwitch(command_line_), std::nullopt);
  command_line_.AppendSwitchASCII(switches::kMaxHttpCacheSize, "16777216");
  EXPECT_EQ(GetMaxHttpCacheSizeSwitch(command_line_), 16777216);
}

TEST_F(CacheQuotasTest, OlderSizeParamsAreIgnoredWithAWarning) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kCobaltCacheQuotas, {}},
       {features::kCobaltHttpCacheQuota,
        {{features::kCobaltMaxHttpCacheSizeBytes.name, "12582912"}}},
       {features::kCobaltCodeCacheOptimization,
        {{features::kCobaltGeneratedCodeCacheSizeBytes.name, "10485760"}}}},
      {});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation quotas =
      ComputeCacheQuotas(command_line_, 24 * kMiB);
  EXPECT_EQ(quotas.sizes, MiB(9, 5, 1, 6, 2, 1));
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kExact);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "CobaltHttpCacheQuota"), 1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING, "CobaltCodeCacheOptimization"),
            1);
  EXPECT_EQ(logs.Count(logging::LOGGING_WARNING), 2);
}

TEST_F(CacheQuotasTest, ProcessQuotasComeFromTheProcessCommandLineAndBudget) {
  EnableWithParams({});
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kMaxHttpCacheSize, "16777216");

  const CacheQuotaAllocation quotas = GetCacheQuotas();
  EXPECT_EQ(quotas.sizes.http_cache, 16 * kMiB);
  EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
  EXPECT_EQ(quotas.platform_budget_bytes,
            int64_t{kSbMaxSystemPathCacheDirectorySize});
}

TEST_F(CacheQuotasTest, ProcessQuotasAreComputedOnce) {
  EnableWithParams({{"CobaltCacheQuotas_http_cache_size_bytes", "4194304"},
                    {"CobaltCacheQuotas_js_code_cache_size_bytes", "10485760"},
                    {"CobaltCacheQuotas_uncommitted_policy", "none"}});
  CacheQuotaLogs logs;
  const CacheQuotaAllocation first = GetCacheQuotas();
  EXPECT_EQ(first.sizes, MiB(4, 10, 1, 6, 2, 1));
  EXPECT_EQ(logs.Count(logging::LOGGING_INFO, "quotas in bytes"), 1);

  // Neither new params nor new switches change the quotas of the process.
  feature_list_.Reset();
  EnableWithParams({{"CobaltCacheQuotas_uncommitted_policy", "none"}});
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ::switches::kGpuDiskCacheSizeKB, "4096");
  EXPECT_EQ(GetCacheQuotas().sizes, first.sizes);
  EXPECT_EQ(logs.Count(logging::LOGGING_INFO, "quotas in bytes"), 1);

  ResetCacheQuotasForTesting();
  EXPECT_EQ(GetCacheQuotas().sizes, MiB(9, 5, 1, 6, 4, 1));
  EXPECT_EQ(logs.Count(logging::LOGGING_INFO, "quotas in bytes"), 2);
}

}  // namespace
}  // namespace cobalt
