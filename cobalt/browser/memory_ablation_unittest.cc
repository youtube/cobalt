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

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class MemoryAblationTest : public ::testing::Test {
 protected:
  void SetUp() override { ResetMemoryAblationForTesting(); }

  void TearDown() override { ResetMemoryAblationForTesting(); }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MemoryAblationTest, DisabledByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCobaltNativeMemoryAblation);

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", false, 1);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 0);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);
}

TEST_F(MemoryAblationTest, EnabledWithZeroSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "0"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 0, 1);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);
}

TEST_F(MemoryAblationTest, EnabledWithAllocatedSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "1"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kSuccess, 1);
}

TEST_F(MemoryAblationTest, EnabledWithExceedingMaxSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation,
      {{"ablation_size_mb", base::NumberToString(kMaxAblationSizeMB + 1)}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB",
      kMaxAblationSizeMB + 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kExceedsMaxLimit, 1);
}

TEST_F(MemoryAblationTest, ExecutesAtMostOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "1"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  // Call second time within same app lifetime
  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  // Histograms should only have 1 sample
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kSuccess, 1);
}

}  // namespace
}  // namespace cobalt
