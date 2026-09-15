// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)
#include "base/features.h"
#include "base/test/scoped_feature_list.h"
#endif

namespace blink {

// Verify desktop memory limit calculations.
#if !BUILDFLAG(IS_ANDROID)
TEST(LayerTreeSettings, IgnoreGivenMemoryPolicy) {
  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
#if BUILDFLAG(IS_STARBOARD)
  EXPECT_EQ(64u * 1024u * 1024u, policy.bytes_limit_when_visible);
#else
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
#endif
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
#if BUILDFLAG(IS_STARBOARD)
  EXPECT_EQ(64u * 1024u * 1024u, policy.bytes_limit_when_visible);
#else
  EXPECT_EQ(977272832u, policy.bytes_limit_when_visible);
#endif
#if BUILDFLAG(IS_COBALT)
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
#else
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
#endif
            policy.priority_cutoff_when_visible);

  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(2056, 1329), 2.f);
#if BUILDFLAG(IS_STARBOARD)
  EXPECT_EQ(64u * 1024u * 1024u, policy.bytes_limit_when_visible);
#else
  EXPECT_EQ(1152u * 1024u * 1024u, policy.bytes_limit_when_visible);
#endif
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
#endif

}  // namespace blink
