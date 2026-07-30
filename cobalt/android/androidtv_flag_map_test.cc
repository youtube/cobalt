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

#include "cobalt/android/androidtv_flag_map.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cobalt/browser/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/display_switches.h"

namespace cobalt {
namespace {

// Test shorthand: builds an AndroidTvFlagMap from "Key1=Val1,Key2".
AndroidTvFlagMap MapFrom(std::string_view spec) {
  std::vector<std::string> key_values;
  for (std::string_view entry : base::SplitStringPiece(
           spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const size_t separator = entry.find('=');
    key_values.emplace_back(entry.substr(0, separator));
    key_values.emplace_back(separator == std::string_view::npos
                                ? std::string_view()
                                : entry.substr(separator + 1));
  }
  return AndroidTvFlagMap::FromKeyValues(key_values);
}

base::CommandLine ExpandWith(std::string_view spec) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ExpandAndroidTvFlags(MapFrom(spec), &cmd_line);
  return cmd_line;
}

TEST(AndroidTvFlagMapTest, FromKeyValues) {
  const AndroidTvFlagMap map =
      AndroidTvFlagMap::FromKeyValues({"KeyA", "1", "KeyB", "", "KeyC"});
  EXPECT_TRUE(map.Has("KeyA"));
  EXPECT_TRUE(map.Has("KeyB"));
  EXPECT_TRUE(map.Has("KeyC"));
  EXPECT_FALSE(map.Has("KeyD"));
  EXPECT_EQ("1", map.GetNumeric("KeyA"));
  EXPECT_FALSE(AndroidTvFlagMap::FromKeyValues({}).Has("KeyA"));
}

TEST(AndroidTvFlagMapTest, GetNumeric) {
  const AndroidTvFlagMap map = AndroidTvFlagMap::FromKeyValues(
      {"Number", "256", "Mixed", "5,0=0", "NoDigits", "abc", "Empty", ""});
  EXPECT_EQ("256", map.GetNumeric("Number"));
  EXPECT_EQ("500", map.GetNumeric("Mixed"));
  EXPECT_EQ(std::nullopt, map.GetNumeric("NoDigits"));
  EXPECT_EQ(std::nullopt, map.GetNumeric("Empty"));
  EXPECT_EQ(std::nullopt, map.GetNumeric("Absent"));
}

TEST(AndroidTvFlagMapTest, NoSwitchesAppliesExperimentDefaults) {
  const base::CommandLine cmd_line = ExpandWith("");

  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kDisableQuic));
  // Low-end device mode is emitted from Java, since it is read before the
  // native initialization hook runs.
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kEnableLowEndDeviceMode));
  EXPECT_EQ("--initial-old-space-size=64,--max-old-space-size=512",
            cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  // The device scale factor is emitted from Java, since it is read before the
  // native initialization hook runs.
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kForceDeviceScaleFactor));
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
  EXPECT_EQ("64", cmd_line.GetSwitchValueASCII(
                      blink::switches::kForceGpuMemAvailableMb));
#else
  EXPECT_FALSE(cmd_line.HasSwitch(blink::switches::kForceGpuMemAvailableMb));
#endif
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kEnableFeatures));
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kDisableFeatures));
  EXPECT_FALSE(cmd_line.HasSwitch(switches::kUseStarboardLifecycle));
}

TEST(AndroidTvFlagMapTest, EnableQuic) {
  const base::CommandLine cmd_line = ExpandWith("EnableQUIC");

  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kDisableQuic));
}

TEST(AndroidTvFlagMapTest, AppliesAllExperiments) {
  const base::CommandLine cmd_line = ExpandWith(
      "AllowCriticalMemoryPressureHandlingInForeground,"
      "AreaBasedVideoBufferBudget,"
      "AvoidCCReuseResource,"
      "CobaltBypassHTMLPreloadScanner,"
      "CobaltBypassResourceLoadScheduler,"
      "CobaltDynamicMojoPipeMediaSize=2048,"
      "CobaltDynamicMojoPipeSubresourceSize=1024,"
      "DecodedImageWorkingSetBudgetBytes=1000000,"
      "DisableGpuMemoryBufferCompositorResources,"
      "DisableLessAggressiveParkableString,"
      "EnableActivityLifecycleCoordination,"
      "EnableCobaltContentLengthAwareMojoPipeSizing,"
      "EnableCobaltDynamicMojoPipeSizing,"
      "EnableCobaltMmapFontCache,"
      "EnableModerateMemoryPressure,"
      "EnableScalingClippedImages,"
      "EvictMemoryCacheOnCriticalMemoryPressure,"
      "ForceGpuMemAvailableMb=256,"
      "GpuImageCacheLimitItems=500,"
      "InterestAreaSizeInPixels=400,"
      "LimitImageDecodeCacheSizeMb=32,"
      "MemoryPressureCooldownInSeconds=15,"
      "ReclaimDelayInSeconds=5,"
      "UseMinorMSForMinorGC,"
      "UseStarboardLifeCycle,"
      "V8DisableSparkplug,"
      "V8InitialOldSpaceSize=128,"
      "V8MaxOldSpaceSize=1024,"
      "V8SetBytecodeOldTime=10");

  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kDisableQuic));
  EXPECT_TRUE(cmd_line.HasSwitch(
      ::switches::kDisableGpuMemoryBufferCompositorResources));
  EXPECT_EQ("256", cmd_line.GetSwitchValueASCII(
                       blink::switches::kForceGpuMemAvailableMb));
  EXPECT_EQ("500",
            cmd_line.GetSwitchValueASCII(::switches::kCCImageCacheLimitItems));
  EXPECT_EQ("32",
            cmd_line.GetSwitchValueASCII(::switches::kCCImageCacheLimitMbs));
  EXPECT_EQ("1000000", cmd_line.GetSwitchValueASCII(
                           ::switches::kDecodedImageWorkingSetBudgetBytes));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kEnableClippedImageScaling));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kAvoidCCReuseResource));
  EXPECT_TRUE(cmd_line.HasSwitch(
      switches::kAllowCriticalMemoryPressureHandlingInForeground));
  EXPECT_TRUE(cmd_line.HasSwitch(switches::kUseStarboardLifecycle));
  EXPECT_TRUE(
      cmd_line.HasSwitch(switches::kEnableActivityLifecycleCoordination));

  EXPECT_EQ(
      "CobaltDynamicMojoPipeSizing:subresource_size/1024/media_size/2048,"
      "CobaltContentLengthAwareMojoPipeSizing,"
      "SmallerInterestArea:size_in_pixels/400/reclaim_delay_s/5,"
      "CobaltBypassResourceLoadScheduler,"
      "CobaltBypassHTMLPreloadScanner,"
      "CobaltMmapFontCache,"
      "AreaBasedVideoBufferBudget,"
      "EvictMemoryCacheOnCriticalMemoryPressure,"
      "CobaltEnableModerateMemoryPressure,"
      "CobaltMemoryPressureCooldown:cooldown-seconds/15",
      cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));
  EXPECT_EQ("LessAggressiveParkableString",
            cmd_line.GetSwitchValueASCII(::switches::kDisableFeatures));
  EXPECT_EQ(
      "--minor-ms,"
      "--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0,"
      "--flush-bytecode,"
      "--bytecode-old-time=10,"
      "--initial-old-space-size=128,"
      "--no-sparkplug,"
      "--max-old-space-size=1024",
      cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
}

TEST(AndroidTvFlagMapTest, MojoPipeSizingWithoutParams) {
  const base::CommandLine cmd_line =
      ExpandWith("EnableCobaltDynamicMojoPipeSizing");
  EXPECT_EQ("CobaltDynamicMojoPipeSizing",
            cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));
}

TEST(AndroidTvFlagMapTest, NonNumericValuesFallBackToDefaults) {
  const base::CommandLine cmd_line =
      ExpandWith("V8InitialOldSpaceSize=abc,GpuImageCacheLimitItems");
  EXPECT_EQ("--initial-old-space-size=64,--max-old-space-size=512",
            cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kCCImageCacheLimitItems));
}

TEST(AndroidTvFlagMapTest, UserProvidedSwitchesTakePrecedence) {
  const char* const input_argv[] = {
      "PROGRAM",
      "--force-gpu-mem-available-mb=128",
      "--enable-features=UserFeature",
      "--disable-features=UserDisabledFeature",
      "--js-flags=--max-old-space-size=2048",
  };
  base::CommandLine cmd_line(static_cast<int>(std::size(input_argv)),
                             input_argv);
  ExpandAndroidTvFlags(
      MapFrom("ForceGpuMemAvailableMb=256,EnableCobaltMmapFontCache,"
              "DisableLessAggressiveParkableString,V8MaxOldSpaceSize=1024"),
      &cmd_line);

  EXPECT_EQ("128", cmd_line.GetSwitchValueASCII(
                       blink::switches::kForceGpuMemAvailableMb));
  EXPECT_EQ("UserFeature,CobaltMmapFontCache",
            cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));
  EXPECT_EQ("UserDisabledFeature,LessAggressiveParkableString",
            cmd_line.GetSwitchValueASCII(::switches::kDisableFeatures));
  // V8 is last-wins, so the user value must come last.
  EXPECT_EQ(
      "--initial-old-space-size=64,--max-old-space-size=1024,"
      "--max-old-space-size=2048",
      cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
}

}  // namespace
}  // namespace cobalt
