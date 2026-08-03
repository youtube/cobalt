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

#include "cobalt/browser/metrics/cobalt_memory_metrics_helper.h"

#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltMemoryMetricsHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // No special initialization required for StatisticsRecorder.
  }
};

TEST_F(CobaltMemoryMetricsHelperTest, HandlesUnrecordedHistogram) {
  auto result = CobaltMemoryMetricsHelper::GetMetricValueBytes(
      "Memory.Experimental.Browser2.NonExistentTestMetric");
  EXPECT_FALSE(result.has_value());
}

TEST_F(CobaltMemoryMetricsHelperTest, RetrievesRecordedHistogramInBytes) {
  const char* test_histogram_name = "Memory.Experimental.Browser2.Malloc";

  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      test_histogram_name, 1, 1000000, 50, base::HistogramBase::kNoFlags);
  histogram->Add(1024);  // 1024 KiB = 1 MB

  auto result =
      CobaltMemoryMetricsHelper::GetMetricValueBytes(test_histogram_name);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1024u * 1024u);  // 1,048,576 Bytes
}

TEST_F(CobaltMemoryMetricsHelperTest,
       GetMemoryBreakdownReturnsRecordedMetrics) {
  const char* metric_name = "Memory.Browser.ResidentSet";
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      metric_name, 1, 1000000, 50, base::HistogramBase::kNoFlags);
  histogram->Add(2048);  // 2048 KiB

  std::vector<MemoryBreakdownMetric> breakdown =
      CobaltMemoryMetricsHelper::GetMemoryBreakdown();

  bool found_resident_set = false;
  for (const auto& entry : breakdown) {
    if (std::string(entry.name) == metric_name) {
      found_resident_set = true;
      EXPECT_GE(entry.value_bytes, 2048u * 1024u);
    }
  }
  EXPECT_TRUE(found_resident_set);
}

}  // namespace cobalt
