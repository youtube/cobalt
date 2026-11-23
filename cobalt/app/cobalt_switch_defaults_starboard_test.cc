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

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/browser/cache_quotas.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt_switch_defaults.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_switches.h"

namespace cobalt {
namespace {

bool HasSwitch(const CommandLinePreprocessor& cmd_line_pxr,
               const char* switch_str) {
  return cmd_line_pxr.get_cmd_line_for_test().HasSwitch(switch_str);
}

std::string GetSwitchValue(const CommandLinePreprocessor& cmd_line_pxr,
                           const char* switch_str) {
  if (HasSwitch(cmd_line_pxr, switch_str)) {
    return cmd_line_pxr.get_cmd_line_for_test().GetSwitchValueASCII(switch_str);
  }
  return "";
}

TEST(CobaltSwitchDefaultsTest, MergeDisabledFeatures) {
  const auto input_argv = std::to_array<const char*>(
      {"PROGRAM", "--disable-features=PersistentOriginTrials"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::string disabled_features =
      GetSwitchValue(cmd_line_pxr, ::switches::kDisableFeatures);
  EXPECT_EQ(std::string("PersistentOriginTrials,Vulkan"), disabled_features);
}

TEST(CobaltSwitchDefaultsTest, MergeEnabledFeatures) {
  const auto input_argv =
      std::to_array<const char*>({"PROGRAM", "--enable-features=UseFoo"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::string enabled_features =
      GetSwitchValue(cmd_line_pxr, ::switches::kEnableFeatures);
  EXPECT_EQ(std::string("UseFoo,DefaultEnableANGLEValidation, "
                        "SmallerInterestArea, "
                        "ReclaimPrepaintTilesWhenIdle, "
                        "ReclaimOldPrepaintTiles, "
                        "WebAudioRemoveAudioDestinationResampler, "
                        "DomStorageSmartFlushing"),
            enabled_features);
}

TEST(CobaltSwitchDefaultsTest, MergeEnabledBlinkFeatures) {
  const auto input_argv =
      std::to_array<const char*>({"PROGRAM", "--enable-blink-features=UseFoo"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  EXPECT_EQ(std::string("UseFoo,PreciseMemoryInfo"),
            GetSwitchValue(cmd_line_pxr, ::switches::kEnableBlinkFeatures));
}

TEST(CobaltSwitchDefaultsTest, MergeJavaScriptFlags) {
  const auto input_argv =
      std::to_array<const char*>({"PROGRAM", "--js-flags=--expose-gc"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  // V8 flags are last-wins, so defaults are prepended and the caller's flags
  // come last.
  EXPECT_EQ(std::string("--no-decommit-pooled-pages --no-concurrent-marking,"
                        "--expose-gc"),
            GetSwitchValue(cmd_line_pxr, blink::switches::kJavaScriptFlags));
}

TEST(CobaltSwitchDefaultsTest, ConsistentWindowSizes) {
  const auto input_argv = std::to_array<const char*>({
      "PROGRAM",
      "--window-size=1280,1024",
  });
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::string content_shell_size =
      (GetSwitchValue(cmd_line_pxr, ::switches::kContentShellHostWindowSize));
  EXPECT_EQ(std::string("1280x1024"), content_shell_size);
}

TEST(CobaltSwitchDefaultsTest, ConsistentWindowSizesOverride) {
  const auto input_argv = std::to_array<const char*>({
      "PROGRAM",
      "--window-size=1280,1024",
      "--content-shell-host-window-size=1920x1080",
  });
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::string content_shell_size =
      (GetSwitchValue(cmd_line_pxr, ::switches::kContentShellHostWindowSize));
  EXPECT_EQ(std::string("1280x1024"), content_shell_size);
}

TEST(CobaltSwitchDefaultsTest, GfxAngleOverride) {
  const auto input_argv = std::to_array<const char*>({
      "PROGRAM",
      "--use-angle=swiftshader",
  });
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::string use_angle_setting =
      GetSwitchValue(cmd_line_pxr, ::switches::kUseANGLE);
  EXPECT_EQ(std::string("swiftshader"), use_angle_setting);
  // Note: This swiftshader argument being respected by the defaults is required
  // for running in Forge environments.
}

TEST(CobaltSwitchDefaultsTest, AlwaysEnabledSwitches) {
  const auto input_argv = std::to_array<const char*>({"PROGRAM"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  std::vector<const char*> always_on_switches{
      ::switches::kSingleProcess,
      ::switches::kIgnoreGpuBlocklist,
      sandbox::policy::switches::kNoSandbox,
      ::switches::kHideScrollbars,
  };

  for (const auto& switch_key : always_on_switches) {
    EXPECT_TRUE(HasSwitch(cmd_line_pxr, switch_key));
  }
  // Other default switches are subject to changes later down the line.
}

TEST(CobaltSwitchDefaultsTest, StartupURLSwitch) {
  const auto input_argv = std::to_array<const char*>({
      "PROGRAM",
      "--url=foo",
  });
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  EXPECT_EQ("foo", cmd_line_pxr.get_startup_url_for_test());
  EXPECT_EQ("foo", GetSwitchValue(cmd_line_pxr, cobalt::switches::kInitialURL));
}

TEST(CobaltSwitchDefaultsTest, StartupURLSwitchAndArg) {
  const auto input_argv =
      std::to_array<const char*>({"PROGRAM", "--url=foo", "bar"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  EXPECT_EQ("bar", cmd_line_pxr.get_startup_url_for_test());
  EXPECT_EQ("bar", GetSwitchValue(cmd_line_pxr, cobalt::switches::kInitialURL));
}

TEST(CobaltSwitchDefaultsTest, StartupURLArg) {
  const auto input_argv = std::to_array<const char*>({"PROGRAM", "data:,"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  EXPECT_EQ("data:,", cmd_line_pxr.get_startup_url_for_test());
  EXPECT_EQ("data:,",
            GetSwitchValue(cmd_line_pxr, cobalt::switches::kInitialURL));
}

TEST(CobaltSwitchDefaultsTest, StartupURLDefault) {
  const auto input_argv = std::to_array<const char*>({"PROGRAM"});
  const int input_argc = static_cast<int>(input_argv.size());
  CommandLinePreprocessor cmd_line_pxr(input_argc, input_argv.data());

  EXPECT_EQ("https://www.youtube.com/tv",
            cmd_line_pxr.get_startup_url_for_test());
  EXPECT_EQ("", GetSwitchValue(cmd_line_pxr, cobalt::switches::kInitialURL));
}

// CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache.
base::CommandLine ApplyPreloadGpuShaderDiskCache(bool is_visible) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line, is_visible);
  return command_line;
}

bool DiskCacheEnabled(const base::CommandLine& command_line) {
  return !command_line.HasSwitch(::switches::kDisableGpuShaderDiskCache);
}

TEST(PreloadGpuShaderDiskCacheTest, PreloadLaunchEnablesGrShaderCacheOnly) {
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_TRUE(DiskCacheEnabled(command_line));
  EXPECT_TRUE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
}

TEST(PreloadGpuShaderDiskCacheTest, PreloadLaunchSetsDefaultCacheSize) {
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            base::NumberToString(
                CobaltMainDelegate::kPreloadGrShaderDiskCacheSizeKB));
}

TEST(PreloadGpuShaderDiskCacheTest, PreloadLaunchRespectsExplicitCacheSize) {
  constexpr char kExplicitSizeKB[] = "512";
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
  command_line.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB,
                                 kExplicitSizeKB);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_TRUE(DiskCacheEnabled(command_line));
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            kExplicitSizeKB);
}

TEST(PreloadGpuShaderDiskCacheTest, VisibleLaunchLeavesCacheDisabled) {
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/true);
  EXPECT_FALSE(DiskCacheEnabled(command_line));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB));
}

TEST(PreloadGpuShaderDiskCacheTest, DisableSwitchLeavesCacheDisabled) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
  command_line.AppendSwitch(::switches::kDisablePreloadGpuShaderDiskCache);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_FALSE(DiskCacheEnabled(command_line));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB));
}

// With kCobaltCacheQuotas the size is the GPU shader cache quota.
class PreloadGpuShaderDiskCacheQuotaTest : public testing::Test {
 protected:
  void SetUp() override { ResetCacheQuotasForTesting(); }
  void TearDown() override { ResetCacheQuotasForTesting(); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PreloadGpuShaderDiskCacheQuotaTest, PreloadLaunchSetsDefaultQuota) {
  feature_list_.InitAndEnableFeature(features::kCobaltCacheQuotas);
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_TRUE(DiskCacheEnabled(command_line));
  EXPECT_TRUE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            "2048");
}

TEST_F(PreloadGpuShaderDiskCacheQuotaTest, PreloadLaunchSetsQuotaFromParam) {
  // 4 MiB for the GPU shader cache, taken from the HTTP cache.
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_gpu_shader_cache_size_bytes", "4194304"},
       {"CobaltCacheQuotas_http_cache_size_bytes", "7340032"}});
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            "4096");
}

// A size of 0 KB would make the GPU process use its default size.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest, PreloadLaunchSetsAtLeastOneKB) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_gpu_shader_cache_size_bytes", "1000"},
       {"CobaltCacheQuotas_uncommitted_policy", "none"}});
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            "1");
}

TEST_F(PreloadGpuShaderDiskCacheQuotaTest,
       PreloadLaunchRespectsExplicitCacheSize) {
  feature_list_.InitAndEnableFeature(features::kCobaltCacheQuotas);
  constexpr char kExplicitSizeKB[] = "512";
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
  command_line.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB,
                                 kExplicitSizeKB);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_TRUE(DiskCacheEnabled(command_line));
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            kExplicitSizeKB);
}

// The size is a whole number of KB.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest, PreloadLaunchRoundsQuotaDown) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kCobaltCacheQuotas,
      {{"CobaltCacheQuotas_gpu_shader_cache_size_bytes", "5000"},
       {"CobaltCacheQuotas_uncommitted_policy", "none"}});
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            "4");
  EXPECT_EQ(GetCacheQuotas().sizes.gpu_shader_cache, 4096);
}

// The GPU process doubles the value of the switch if
// kAggressiveShaderCacheLimits is enabled, so half the quota is passed.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest,
       PreloadLaunchHalvesQuotaForAggressiveShaderCacheLimits) {
  feature_list_.InitWithFeatures(
      {features::kCobaltCacheQuotas, ::features::kAggressiveShaderCacheLimits},
      {});
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false);
  EXPECT_EQ(command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
            "1024");
  EXPECT_EQ(GetCacheQuotas().sizes.gpu_shader_cache, 2 * 1024 * 1024);
}

// Whatever the quota, the size that the GPU process computes from the command
// line of a preload launch is the GPU shader cache quota that is reported.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest,
       PreloadLaunchGivesTheGpuProcessTheReportedQuota) {
  for (bool aggressive_limits : {false, true}) {
    for (const char* param_bytes :
         {"2097152", "4194304", "3145728", "2098176", "5000", "3072", "1000"}) {
      SCOPED_TRACE(testing::Message() << param_bytes << " bytes, aggressive "
                                      << aggressive_limits);
      base::test::ScopedFeatureList feature_list;
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
      feature_list.InitWithFeaturesAndParameters(enabled, disabled);
      ResetCacheQuotasForTesting();

      // As in the app, the switches are those of the process.
      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitch(::switches::kDisableGpuShaderDiskCache);
      CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(command_line,
                                                         /*is_visible=*/false);

      const CacheQuotaAllocation quotas = GetCacheQuotas();
      EXPECT_NE(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
      EXPECT_EQ(
          base::saturated_cast<int64_t>(gpu::GetDefaultGpuDiskCacheSize()),
          quotas.sizes.gpu_shader_cache);
      // At most 1 KB is lost to rounding, or 2 KB if the size is doubled, and
      // at most 2 KB are added to reach the smallest size.
      int64_t param = 0;
      ASSERT_TRUE(base::StringToInt64(param_bytes, &param));
      EXPECT_GT(quotas.sizes.gpu_shader_cache, param - 2048);
      EXPECT_LE(quotas.sizes.gpu_shader_cache, std::max<int64_t>(param, 2048));
    }
  }
}

// The GPU process would use its default size for a switch that is not a
// number, so it is replaced by the quota. This is the only switch of the user
// that is changed.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest,
       PreloadLaunchReplacesCacheSizeThatIsNotANumber) {
  for (bool aggressive_limits : {false, true}) {
    for (const char* value : {"", "abc", "-1", "2048kb"}) {
      SCOPED_TRACE(testing::Message()
                   << "\"" << value << "\", aggressive " << aggressive_limits);
      base::test::ScopedFeatureList feature_list;
      if (aggressive_limits) {
        feature_list.InitWithFeatures(
            {features::kCobaltCacheQuotas,
             ::features::kAggressiveShaderCacheLimits},
            {});
      } else {
        feature_list.InitWithFeatures(
            {features::kCobaltCacheQuotas},
            {::features::kAggressiveShaderCacheLimits});
      }
      ResetCacheQuotasForTesting();

      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitch(::switches::kDisableGpuShaderDiskCache);
      command_line->AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, value);
      CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(command_line,
                                                         /*is_visible=*/false);

      EXPECT_TRUE(DiskCacheEnabled(*command_line));
      EXPECT_EQ(
          command_line->GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
          aggressive_limits ? "1024" : "2048");
      // The value of the user is gone from the command line, which is also
      // logged and handed on as a string.
      EXPECT_EQ(std::ranges::count_if(
                    command_line->argv(),
                    [](const base::CommandLine::StringType& arg) {
                      return arg.starts_with("--gpu-disk-cache-size-kb");
                    }),
                1);
      // The switch was ignored, so it is not a developer override, and the GPU
      // process uses the allocated quota.
      const CacheQuotaAllocation quotas = GetCacheQuotas();
      EXPECT_NE(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
      EXPECT_EQ(quotas.sizes.gpu_shader_cache, 2 * 1024 * 1024);
      EXPECT_EQ(
          base::saturated_cast<int64_t>(gpu::GetDefaultGpuDiskCacheSize()),
          quotas.sizes.gpu_shader_cache);
    }
  }
}

// A switch that is a number is the user's and stays, 0 included: 0 hands the
// size to the GPU process, and that size is reported.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest,
       PreloadLaunchKeepsCacheSizeThatIsANumber) {
  for (bool aggressive_limits : {false, true}) {
    for (const char* value : {"0", "512", "4096"}) {
      SCOPED_TRACE(testing::Message()
                   << "\"" << value << "\", aggressive " << aggressive_limits);
      base::test::ScopedFeatureList feature_list;
      if (aggressive_limits) {
        feature_list.InitWithFeatures(
            {features::kCobaltCacheQuotas,
             ::features::kAggressiveShaderCacheLimits},
            {});
      } else {
        feature_list.InitWithFeatures(
            {features::kCobaltCacheQuotas},
            {::features::kAggressiveShaderCacheLimits});
      }
      ResetCacheQuotasForTesting();

      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitch(::switches::kDisableGpuShaderDiskCache);
      command_line->AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, value);
      CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(command_line,
                                                         /*is_visible=*/false);

      EXPECT_EQ(
          command_line->GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
          value);
      const CacheQuotaAllocation quotas = GetCacheQuotas();
      EXPECT_EQ(quotas.outcome, CacheQuotaOutcome::kDeveloperOverride);
      EXPECT_EQ(
          base::saturated_cast<int64_t>(gpu::GetDefaultGpuDiskCacheSize()),
          quotas.sizes.gpu_shader_cache);
    }
  }
}

// Without kCobaltCacheQuotas nothing makes up for the doubling, and a switch
// of the user is never changed.
TEST_F(PreloadGpuShaderDiskCacheQuotaTest, DisabledFeatureIsUnchanged) {
  for (bool aggressive_limits : {false, true}) {
    SCOPED_TRACE(aggressive_limits);
    base::test::ScopedFeatureList feature_list;
    if (aggressive_limits) {
      feature_list.InitWithFeatures({::features::kAggressiveShaderCacheLimits},
                                    {features::kCobaltCacheQuotas});
    } else {
      feature_list.InitWithFeatures({},
                                    {features::kCobaltCacheQuotas,
                                     ::features::kAggressiveShaderCacheLimits});
    }
    EXPECT_EQ(ApplyPreloadGpuShaderDiskCache(/*is_visible=*/false)
                  .GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
              "2048");

    for (const char* value : {"", "abc", "0", "512"}) {
      SCOPED_TRACE(value);
      base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
      command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
      command_line.AppendSwitchASCII(::switches::kGpuDiskCacheSizeKB, value);
      CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                         /*is_visible=*/false);
      EXPECT_TRUE(DiskCacheEnabled(command_line));
      EXPECT_EQ(
          command_line.GetSwitchValueASCII(::switches::kGpuDiskCacheSizeKB),
          value);
    }
  }
}

TEST_F(PreloadGpuShaderDiskCacheQuotaTest, VisibleLaunchLeavesCacheDisabled) {
  feature_list_.InitAndEnableFeature(features::kCobaltCacheQuotas);
  const base::CommandLine command_line =
      ApplyPreloadGpuShaderDiskCache(/*is_visible=*/true);
  EXPECT_FALSE(DiskCacheEnabled(command_line));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB));
}

TEST_F(PreloadGpuShaderDiskCacheQuotaTest, DisableSwitchLeavesCacheDisabled) {
  feature_list_.InitAndEnableFeature(features::kCobaltCacheQuotas);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kDisableGpuShaderDiskCache);
  command_line.AppendSwitch(::switches::kDisablePreloadGpuShaderDiskCache);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_FALSE(DiskCacheEnabled(command_line));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuShaderDiskCacheGrOnly));
  EXPECT_FALSE(command_line.HasSwitch(::switches::kGpuDiskCacheSizeKB));
}

}  // namespace
}  // namespace cobalt
