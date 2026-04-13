// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

TEST(DetailedMetricsTest, DefaultValues) {
  DetailedMetrics metrics;
  EXPECT_TRUE(metrics.categories_kb.empty());
  EXPECT_EQ(metrics.total_pss_kb, 0u);
  EXPECT_EQ(metrics.total_rss_kb, 0u);
}

TEST(DetailedMetricsTest, MoveConstruct) {
  DetailedMetrics metrics;
  metrics.categories_kb["cat1"] = 100;
  metrics.total_pss_kb = 100;
  metrics.total_rss_kb = 200;

  DetailedMetrics moved_metrics(std::move(metrics));
  EXPECT_EQ(moved_metrics.categories_kb["cat1"], 100u);
  EXPECT_EQ(moved_metrics.total_pss_kb, 100u);
  EXPECT_EQ(moved_metrics.total_rss_kb, 200u);
}

class MockDetailedMetricsDelegate : public DetailedMetricsDelegate {
 public:
  MOCK_METHOD(bool, OnSmapsBuffer, (std::string_view buffer), (override));
  MOCK_METHOD(DetailedMetrics, GetAndResetStats, (), (override));
};

TEST(DetailedMetricsDelegateTest, MockUsage) {
  MockDetailedMetricsDelegate delegate;
  EXPECT_CALL(delegate, OnSmapsBuffer("test buffer"))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(delegate.OnSmapsBuffer("test buffer"));

  DetailedMetrics metrics;
  metrics.categories_kb["test"] = 42;
  EXPECT_CALL(delegate, GetAndResetStats())
      .WillOnce(testing::Return(testing::ByMove(std::move(metrics))));

  DetailedMetrics result = delegate.GetAndResetStats();
  EXPECT_EQ(result.categories_kb["test"], 42u);
}

}  // namespace memory_instrumentation
