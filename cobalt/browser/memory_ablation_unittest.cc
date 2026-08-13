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

#include "cobalt/browser/memory_ablation.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

TEST(MemoryAblationTest, DisabledByDefault) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCobaltNativeMemoryAblation);

  EXPECT_EQ(MaybeApplyMemoryAblation(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", false, 1);
}

TEST(MemoryAblationTest, EnabledWithZeroSize) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "0"}});

  EXPECT_EQ(MaybeApplyMemoryAblation(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 0, 1);
}

TEST(MemoryAblationTest, EnabledWithAllocatedSize) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "1"}});

  EXPECT_EQ(MaybeApplyMemoryAblation(), 1u);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
}

}  // namespace
}  // namespace cobalt
