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

#include "cobalt/app/cobalt_java_switches_android.h"

#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cobalt/browser/constants/cobalt_java_switch_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/display_switches.h"

namespace cobalt {
namespace {

TEST(CobaltJavaSwitchesAndroidTest, DefaultSwitchesWhenNoJavaSwitchesPresent) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  ApplyJavaSwitches(&cmd_line);

  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kDisableQuic));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kEnableLowEndDeviceMode));
  EXPECT_EQ("--initial-old-space-size=64,--max-old-space-size=512",
            cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  EXPECT_EQ("1",
            cmd_line.GetSwitchValueASCII(::switches::kForceDeviceScaleFactor));
#if !defined(ARCH_CPU_ARM64) && !defined(ARCH_CPU_X86_64)
  EXPECT_EQ("64",
            cmd_line.GetSwitchValueASCII(::switches::kForceGpuMemAvailableMb));
#else
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kForceGpuMemAvailableMb));
#endif
}

TEST(CobaltJavaSwitchesAndroidTest, DisableLowEndDeviceModeSuppressesSwitch) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(kCobaltJavaSwitches, "DisableLowEndDeviceMode=1");
  ApplyJavaSwitches(&cmd_line);

  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kEnableLowEndDeviceMode));
}

TEST(CobaltJavaSwitchesAndroidTest, AppliesAllSerializedJavaSwitches) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  // Pre-populate default features and js-flags as CommandLineOverrideHelper
  // does prior to library load.
  cmd_line.AppendSwitchASCII(::switches::kEnableFeatures,
                             "LogJsConsoleMessages,SmallerInterestArea");
  cmd_line.AppendSwitchASCII(::switches::kDisableFeatures,
                             "PartitionAllocBackupRefPtr");
  cmd_line.AppendSwitchASCII(
      blink::switches::kJavaScriptFlags,
      "--no-decommit-pooled-pages,--no-concurrent-marking");

  const std::string serialized = base::JoinString(
      {
          "EnableQUIC=1",
          "UseMinorMSForMinorGC=1",
          "V8SetBytecodeOldTime=10",
          "V8InitialOldSpaceSize=128",
          "V8DisableSparkplug=1",
          "V8MaxOldSpaceSize=1024",
          "ForceGpuMemAvailableMb=256",
          "DisableGpuMemoryBufferCompositorResources=1",
          "GpuImageCacheLimitItems=500",
          "LimitImageDecodeCacheSizeMb=32",
          "DecodedImageWorkingSetBudgetBytes=1000000",
          "EnableScalingClippedImages=1",
          "EnableCobaltDynamicMojoPipeSizing=1",
          "CobaltDynamicMojoPipeSubresourceSize=1024",
          "CobaltDynamicMojoPipeMediaSize=2048",
          "EnableCobaltContentLengthAwareMojoPipeSizing=1",
          "InterestAreaSizeInPixels=400",
          "ReclaimDelayInSeconds=5",
          "AvoidCCReuseResource=1",
          "CobaltBypassResourceLoadScheduler=1",
          "CobaltBypassHTMLPreloadScanner=1",
          "EnableCobaltMmapFontCache=1",
          "AreaBasedVideoBufferBudget=1",
          "AllowCriticalMemoryPressureHandlingInForeground=1",
          "EvictMemoryCacheOnCriticalMemoryPressure=1",
          "DisableLessAggressiveParkableString=1",
          "EnableModerateMemoryPressure=1",
          "MemoryPressureCooldownInSeconds=30",
          "UseStarboardLifeCycle=1",
          "Force720pUiOn1GbDevices=1",
      },
      ",");
  cmd_line.AppendSwitchASCII(kCobaltJavaSwitches, serialized);

  ApplyJavaSwitches(&cmd_line);

  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kDisableQuic));
  EXPECT_TRUE(cmd_line.HasSwitch(
      ::switches::kDisableGpuMemoryBufferCompositorResources));
  EXPECT_EQ("256",
            cmd_line.GetSwitchValueASCII(::switches::kForceGpuMemAvailableMb));
  EXPECT_EQ("500",
            cmd_line.GetSwitchValueASCII(::switches::kCCImageCacheLimitItems));
  EXPECT_EQ("32",
            cmd_line.GetSwitchValueASCII(::switches::kCCImageCacheLimitMbs));
  EXPECT_EQ("1000000", cmd_line.GetSwitchValueASCII(
                           ::switches::kDecodedImageWorkingSetBudgetBytes));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kEnableClippedImageScaling));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kAvoidCCReuseResource));
  EXPECT_TRUE(cmd_line.HasSwitch(
      ::switches::kAllowCriticalMemoryPressureHandlingInForeground));
  EXPECT_TRUE(cmd_line.HasSwitch(kUseStarboardLifecycleSwitch));
  EXPECT_EQ("1.5",
            cmd_line.GetSwitchValueASCII(::switches::kForceDeviceScaleFactor));

  // Verify JavaSwitches features are prepended ahead of existing default
  // features so parameterized SmallerInterestArea wins.
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
      "CobaltMemoryPressureCooldown:cooldown-seconds/30,"
      "LogJsConsoleMessages,"
      "SmallerInterestArea",
      cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));

  EXPECT_EQ("LessAggressiveParkableString,PartitionAllocBackupRefPtr",
            cmd_line.GetSwitchValueASCII(::switches::kDisableFeatures));

  // Verify V8 flags are appended after existing default V8 flags.
  EXPECT_EQ(
      "--no-decommit-pooled-pages,--no-concurrent-marking,"
      "--minor-ms,"
      "--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0,"
      "--flush-bytecode,--bytecode-old-time=10,"
      "--initial-old-space-size=128,"
      "--no-sparkplug,"
      "--max-old-space-size=1024",
      cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
}

TEST(CobaltJavaSwitchesAndroidTest, SanitizesNonNumericValues) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitchASCII(
      kCobaltJavaSwitches,
      "V8InitialOldSpaceSize=invalid,V8MaxOldSpaceSize=abc,"
      "ForceGpuMemAvailableMb=none,GpuImageCacheLimitItems=xyz");
  ApplyJavaSwitches(&cmd_line);

  EXPECT_EQ("--initial-old-space-size=64,--max-old-space-size=512",
            cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kCCImageCacheLimitItems));
}

}  // namespace
}  // namespace cobalt
