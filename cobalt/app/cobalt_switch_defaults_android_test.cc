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

#include "cobalt/app/cobalt_switch_defaults.h"

#include <array>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cobalt/browser/switches.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

namespace cobalt {
namespace {

TEST(CobaltSwitchDefaultsAndroidTest, DefaultSwitchesApplied) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  CommandLinePreprocessor::ApplyDefaults(&cmd_line);

  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kSingleProcess));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kForceVideoOverlays));
  EXPECT_TRUE(cmd_line.HasSwitch(blink::switches::kDisableRGBA4444Textures));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kDisableAcceleratedVideoDecode));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kDisableAcceleratedVideoEncode));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kHideScrollbars));
  EXPECT_TRUE(cmd_line.HasSwitch(::switches::kUseCustomAndroidFontsXml));

  EXPECT_EQ("no-user-gesture-required",
            cmd_line.GetSwitchValueASCII(::switches::kAutoplayPolicy));
  EXPECT_EQ("2", cmd_line.GetSwitchValueASCII(::switches::kNumRasterThreads));
  EXPECT_EQ("26214400",
            cmd_line.GetSwitchValueASCII(switches::kMaxHttpCacheSize));
#if !defined(ARCH_CPU_ARM64)
  EXPECT_EQ("gles", cmd_line.GetSwitchValueASCII(::switches::kUseANGLE));
#else
  EXPECT_FALSE(cmd_line.HasSwitch(::switches::kUseANGLE));
#endif
  EXPECT_EQ("--no-decommit-pooled-pages,--no-concurrent-marking",
            cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  EXPECT_EQ("PreciseMemoryInfo",
            cmd_line.GetSwitchValueASCII(::switches::kEnableBlinkFeatures));
  EXPECT_EQ(
      "LogJsConsoleMessages,"
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
      "DefaultPassthroughCommandDecoder,"
#endif
      "SmallerInterestArea,"
      "ReclaimPrepaintTilesWhenIdle,"
      "ReclaimOldPrepaintTiles,"
      "WebAudioRemoveAudioDestinationResampler,"
      "DomStorageSmartFlushing",
      cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));
  EXPECT_EQ(
      "PartitionAllocBackupRefPtr,"
      "UseAAudioInput,"
      "FontSrcLocalMatching,"
      "DeferAudioFocusUntilAudible",
      cmd_line.GetSwitchValueASCII(::switches::kDisableFeatures));
}

TEST(CobaltSwitchDefaultsAndroidTest, MergeFeaturesAndFlagsWithUserOverrides) {
  const auto input_argv = std::to_array<const char*>({
      "PROGRAM",
      "--enable-features=TestFeature1,TestFeature2",
      "--disable-features=TestFeature3",
      "--js-flags=--test-flag,--another-flag",
      "--enable-blink-features=TestBlinkFeature",
      "--max-http-cache-size=1048576",
  });
  base::CommandLine cmd_line(static_cast<int>(input_argv.size()),
                             input_argv.data());
  CommandLinePreprocessor::ApplyDefaults(&cmd_line);

  EXPECT_EQ(
      "TestFeature1,TestFeature2,"
      "LogJsConsoleMessages,"
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
      "DefaultPassthroughCommandDecoder,"
#endif
      "SmallerInterestArea,"
      "ReclaimPrepaintTilesWhenIdle,"
      "ReclaimOldPrepaintTiles,"
      "WebAudioRemoveAudioDestinationResampler,"
      "DomStorageSmartFlushing",
      cmd_line.GetSwitchValueASCII(::switches::kEnableFeatures));
  EXPECT_EQ(
      "TestFeature3,"
      "PartitionAllocBackupRefPtr,"
      "UseAAudioInput,"
      "FontSrcLocalMatching,"
      "DeferAudioFocusUntilAudible",
      cmd_line.GetSwitchValueASCII(::switches::kDisableFeatures));
  EXPECT_EQ(
      "--no-decommit-pooled-pages,--no-concurrent-marking,"
      "--test-flag,--another-flag",
      cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags));
  EXPECT_EQ("TestBlinkFeature,PreciseMemoryInfo",
            cmd_line.GetSwitchValueASCII(::switches::kEnableBlinkFeatures));
  EXPECT_EQ("1048576",
            cmd_line.GetSwitchValueASCII(switches::kMaxHttpCacheSize));
}

}  // namespace
}  // namespace cobalt
