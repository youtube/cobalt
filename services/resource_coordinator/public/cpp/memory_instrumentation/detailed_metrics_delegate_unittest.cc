// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

TEST(SmapsMetricsTest, DefaultValues) {
  SmapsMetrics metrics;
  EXPECT_EQ(metrics.rss_kb, 0u);
  EXPECT_EQ(metrics.pss_kb, 0u);
  EXPECT_EQ(metrics.private_dirty_kb, 0u);
  EXPECT_EQ(metrics.private_clean_kb, 0u);
  EXPECT_EQ(metrics.shared_dirty_kb, 0u);
  EXPECT_EQ(metrics.shared_clean_kb, 0u);
  EXPECT_EQ(metrics.swap_kb, 0u);
  EXPECT_EQ(metrics.swap_pss_kb, 0u);
}

class MockDetailedMetricsDelegate : public DetailedMetricsDelegate {
 public:
  MOCK_METHOD(void, OnSmapsEntry, (absl::string_view name, const SmapsMetrics& metrics), (override));
  MOCK_METHOD(void, GetAndResetStats, ((base::flat_map<std::string, uint64_t>& stats)), (override));
  MOCK_METHOD(base::WeakPtr<DetailedMetricsDelegate>, GetWeakPtr, (), (override));
};

TEST(DetailedMetricsDelegateTest, MockUsage) {
  MockDetailedMetricsDelegate delegate;
  SmapsMetrics metrics;
  metrics.pss_kb = 42;
  
  EXPECT_CALL(delegate, OnSmapsEntry(absl::string_view("test"), testing::_));
  delegate.OnSmapsEntry("test", metrics);

  EXPECT_CALL(delegate, GetAndResetStats(testing::_));
  base::flat_map<std::string, uint64_t> stats;
  delegate.GetAndResetStats(stats);
}

}  // namespace memory_instrumentation
