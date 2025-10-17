// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/metrics/quick_insert_feature_usage_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Optional;

class QuickInsertFeatureUsageMetricsTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(QuickInsertFeatureUsageMetricsTest, IsAlwaysEligible) {
  QuickInsertFeatureUsageMetrics metrics;

  EXPECT_TRUE(metrics.IsEnabled());
}

TEST_F(QuickInsertFeatureUsageMetricsTest, IsAlwaysAccessible) {
  QuickInsertFeatureUsageMetrics metrics;

  EXPECT_THAT(metrics.IsAccessible(), Optional(true));
}

TEST_F(QuickInsertFeatureUsageMetricsTest, IsAlwaysEnabled) {
  QuickInsertFeatureUsageMetrics metrics;

  EXPECT_EQ(metrics.IsEnabled(), true);
}

TEST_F(QuickInsertFeatureUsageMetricsTest, RecordsUsageEvent) {
  base::HistogramTester histogram_tester;
  QuickInsertFeatureUsageMetrics metrics;

  metrics.StartUsage();

  histogram_tester.ExpectBucketCount(
      "ChromeOS.FeatureUsage.Picker",
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.FeatureUsage.Picker",
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure),
      0);
}

TEST_F(QuickInsertFeatureUsageMetricsTest, RecordsUsageTime) {
  base::HistogramTester histogram_tester;
  QuickInsertFeatureUsageMetrics metrics;

  metrics.StartUsage();
  task_environment().FastForwardBy(base::Seconds(1));
  metrics.StopUsage();

  histogram_tester.ExpectTimeBucketCount("ChromeOS.FeatureUsage.Picker.Usetime",
                                         base::Seconds(1), 1);
}

}  // namespace
}  // namespace ash
