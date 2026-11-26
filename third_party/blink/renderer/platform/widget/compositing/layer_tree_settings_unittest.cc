// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)
#include <limits>

#include "base/base_switches.h"
#include "base/features.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#endif

namespace blink {

// Verify desktop memory limit calculations.
//
// These tests are disabled on Starboard because GetGpuMemoryPolicy() may
// override |bytes_limit_when_visible| with a Cobalt-specific budget (see
// base::features::kCobaltForceGpuMemAvailable and the 32-bit default cap), so
// the expected values below are not stable across Starboard configurations.
// Cobalt-specific coverage lives in CobaltForceGpuMemAvailableFeature.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
TEST(LayerTreeSettings, IgnoreGivenMemoryPolicy) {
  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
#if BUILDFLAG(IS_COBALT)
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
#else
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
#endif
            policy.priority_cutoff_when_visible);
}

TEST(LayerTreeSettings, LargeScreensUseMoreMemory) {
  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(4096, 2160), 1.f);
  EXPECT_EQ(977272832u, policy.bytes_limit_when_visible);
#if BUILDFLAG(IS_COBALT)
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
#else
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
#endif
            policy.priority_cutoff_when_visible);

  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(1152u * 1024u * 1024u, policy.bytes_limit_when_visible);
#if BUILDFLAG(IS_COBALT)
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
#else
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
#endif
            policy.priority_cutoff_when_visible);
}
#endif

#if BUILDFLAG(IS_COBALT)
TEST(LayerTreeSettings, CobaltForceGpuMemAvailableFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kCobaltForceGpuMemAvailable,
      {{"force_gpu_mem_available_mb", "128"}});
  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
  EXPECT_EQ(128u * 1024u * 1024u, policy.bytes_limit_when_visible);
}

TEST(LayerTreeSettings, CobaltDefaultGpuMemoryLimitStepsWithResolution) {
  constexpr int kPhysicalMemoryMB = 2048;
  // Unknown screens and screens up to 1080p get 64 MB.
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(), 1.f,
                                                  kPhysicalMemoryMB));
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(1280, 720), 1.f,
                                                  kPhysicalMemoryMB));
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(1920, 1080), 1.f,
                                                  kPhysicalMemoryMB));
  // Above 1080p and up to 1440p get 128 MB.
  EXPECT_EQ(128u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(1921, 1080), 1.f,
                                                   kPhysicalMemoryMB));
  EXPECT_EQ(128u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(2560, 1440), 1.f,
                                                   kPhysicalMemoryMB));
  // Above 1440p get 256 MB.
  EXPECT_EQ(256u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(2561, 1440), 1.f,
                                                   kPhysicalMemoryMB));
  EXPECT_EQ(256u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 1.f,
                                                   kPhysicalMemoryMB));
  EXPECT_EQ(256u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(7680, 4320), 1.f,
                                                   kPhysicalMemoryMB));
  // The device scale factor is applied to get physical pixels.
  EXPECT_EQ(256u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(1920, 1080), 2.f,
                                                   kPhysicalMemoryMB));
}

TEST(LayerTreeSettings, CobaltDefaultGpuMemoryLimitCappedByDeviceMemory) {
  // 1/8 of the physical memory is the cap.
  EXPECT_EQ(128u,
            GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 1.f,
                                             /*physical_memory_mb=*/1024));
  EXPECT_EQ(128u,
            GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(2560, 1440), 1.f,
                                             /*physical_memory_mb=*/1024));
  EXPECT_EQ(192u,
            GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 1.f,
                                             /*physical_memory_mb=*/1536));
  // The 1080p baseline is kept even on devices with very little memory.
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 1.f,
                                                  /*physical_memory_mb=*/256));
  // Unknown device memory does not cap the budget.
  EXPECT_EQ(256u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 1.f,
                                                   /*physical_memory_mb=*/0));
}

TEST(LayerTreeSettings, CobaltDefaultGpuMemoryLimitInvalidInputs) {
  constexpr int kPhysicalMemoryMB = 2048;
  // Invalid device scale factors fall back to the 1080p budget.
  EXPECT_EQ(64u,
            GetCobaltDefaultGpuMemoryLimitMB(
                gfx::Size(3840, 2160), std::numeric_limits<float>::quiet_NaN(),
                kPhysicalMemoryMB));
  EXPECT_EQ(64u,
            GetCobaltDefaultGpuMemoryLimitMB(
                gfx::Size(3840, 2160), std::numeric_limits<float>::infinity(),
                kPhysicalMemoryMB));
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), 0.f,
                                                  kPhysicalMemoryMB));
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 2160), -1.f,
                                                  kPhysicalMemoryMB));
  // So does an empty screen in either dimension.
  EXPECT_EQ(64u, GetCobaltDefaultGpuMemoryLimitMB(gfx::Size(3840, 0), 1.f,
                                                  kPhysicalMemoryMB));
}

#if defined(ARCH_CPU_32_BITS)
// Verifies that 32-bit devices with 2 GB of RAM still get the 256 MB 4K tile
// budget when Cobalt runs with --enable-low-end-device-mode, which caps
// base::SysInfo::AmountOfPhysicalMemoryMB() at 512 MB.
TEST(LayerTreeSettings, CobaltDefaultGpuMemoryPolicyOn32Bit) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ::switches::kEnableLowEndDeviceMode);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(2048);
  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(1920, 1080), 1.f);
  EXPECT_EQ(64u * 1024u * 1024u, policy.bytes_limit_when_visible);
  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(3840, 2160), 1.f);
  EXPECT_EQ(256u * 1024u * 1024u, policy.bytes_limit_when_visible);

  // The Finch feature overrides the resolution-based default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kCobaltForceGpuMemAvailable,
      {{"force_gpu_mem_available_mb", "96"}});
  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(3840, 2160), 1.f);
  EXPECT_EQ(96u * 1024u * 1024u, policy.bytes_limit_when_visible);
}
#endif  // defined(ARCH_CPU_32_BITS)

TEST(LayerTreeSettings, CobaltAnimationTileLookaheadSettings) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{::features::kSmallerInterestArea, {}},
         {base::features::kCobaltSkewportTargetTime, {}},
         {base::features::kCobaltAnimationTileLookahead,
          {{"max_viewport_fraction", "0.4"}}}},
        {});
    cc::LayerTreeSettings settings_1080p = GenerateLayerTreeSettings(
        /*is_threaded=*/true, /*is_for_embedded_frame=*/false,
        /*is_for_scalable_page=*/false, gfx::Size(1920, 1080), 1.f);
    EXPECT_FLOAT_EQ(0.3f, settings_1080p.skewport_target_time_in_seconds);
    EXPECT_FLOAT_EQ(
        0.3f, settings_1080p.gpu_rasterization_skewport_target_time_in_seconds);
    EXPECT_EQ(432,
              settings_1080p.skewport_extrapolation_limit_in_screen_pixels);
    EXPECT_EQ(500, settings_1080p.tiling_interest_area_padding);

    cc::LayerTreeSettings settings_4k = GenerateLayerTreeSettings(
        /*is_threaded=*/true, /*is_for_embedded_frame=*/false,
        /*is_for_scalable_page=*/false, gfx::Size(3840, 2160), 1.f);
    EXPECT_EQ(864, settings_4k.skewport_extrapolation_limit_in_screen_pixels);
    EXPECT_EQ(994, settings_4k.tiling_interest_area_padding);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{base::features::kCobaltSkewportTargetTime,
          {{"skewport_target_time_in_seconds", "0.6"},
           {"gpu_rasterization_skewport_target_time_in_seconds", "0.5"}}},
         {base::features::kCobaltAnimationTileLookahead, {}}},
        {});
    cc::LayerTreeSettings settings = GenerateLayerTreeSettings(
        /*is_threaded=*/true, /*is_for_embedded_frame=*/false,
        /*is_for_scalable_page=*/false, gfx::Size(1920, 1080), 1.f);
    EXPECT_FLOAT_EQ(0.6f, settings.skewport_target_time_in_seconds);
    EXPECT_FLOAT_EQ(0.5f,
                    settings.gpu_rasterization_skewport_target_time_in_seconds);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {base::features::kCobaltSkewportTargetTime},
        {base::features::kCobaltAnimationTileLookahead});
    cc::LayerTreeSettings settings_disabled = GenerateLayerTreeSettings(
        /*is_threaded=*/true, /*is_for_embedded_frame=*/false,
        /*is_for_scalable_page=*/false, gfx::Size(1920, 1080), 1.f);
    EXPECT_FLOAT_EQ(0.0f, settings_disabled.skewport_target_time_in_seconds);
    EXPECT_FLOAT_EQ(
        0.0f,
        settings_disabled.gpu_rasterization_skewport_target_time_in_seconds);
  }
}
#endif

}  // namespace blink
