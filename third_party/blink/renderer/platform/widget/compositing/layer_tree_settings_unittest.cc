// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)
#include "base/features.h"
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

TEST(LayerTreeSettings, CobaltAnimationTileLookaheadSettings) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{::features::kSmallerInterestArea, {}},
         {base::features::kCobaltSkewportTargetTime, {}},
         {base::features::kCobaltAnimationTileLookahead,
          {{"target_time_seconds", "0.3"}, {"max_viewport_fraction", "0.4"}}}},
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
         {base::features::kCobaltAnimationTileLookahead,
          {{"target_time_seconds", "0.3"}}}},
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
