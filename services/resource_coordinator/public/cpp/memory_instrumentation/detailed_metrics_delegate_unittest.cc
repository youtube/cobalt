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
  MOCK_METHOD(void, GetAndResetStats, ((base::flat_map<std::string, uint64_t>* stats)), (override));
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
  delegate.GetAndResetStats(&stats);
}

}  // namespace memory_instrumentation
