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

#include "third_party/blink/renderer/core/inspector/cobalt_memory_metrics_helper.h"

#include <memory>
#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CobaltMemoryMetricsHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    statistics_recorder_ =
        base::StatisticsRecorder::CreateTemporaryForTesting();
  }

  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
};

TEST_F(CobaltMemoryMetricsHelperTest, HandlesUnrecordedHistogram) {
  auto result = GetP50MetricValueBytes(
      "Memory.Experimental.Browser2.NonExistentTestMetric");
  EXPECT_FALSE(result.has_value());
}

TEST_F(CobaltMemoryMetricsHelperTest, RetrievesRecordedHistogramInBytes) {
  const char* test_histogram_name = "Memory.Experimental.Browser2.Malloc";

  base::UmaHistogramMemoryLargeMB(test_histogram_name, 16);  // 16 MiB

  auto result = GetP50MetricValueBytes(test_histogram_name);
  ASSERT_TRUE(result.has_value());
  EXPECT_GE(*result, 10u * 1024u * 1024u);
  EXPECT_LE(*result, 20u * 1024u * 1024u);
}

TEST_F(CobaltMemoryMetricsHelperTest,
       GetP50MemoryBreakdownReturnsRecordedMetricsWithP50Suffix) {
  const char* metric_name = "Memory.Browser.ResidentSet";
  std::string expected_p50_name =
      base::StrCat({metric_name, kP50Suffix});
  base::UmaHistogramMemoryLargeMB(metric_name, 32);  // 32 MiB

  auto breakdown = GetP50MemoryBreakdown();
  ASSERT_TRUE(breakdown.has_value());

  bool found_resident_set_p50 = false;
  for (const auto& entry : *breakdown) {
    if (entry.name == expected_p50_name) {
      found_resident_set_p50 = true;
      EXPECT_GE(entry.value_bytes, 20u * 1024u * 1024u);
      EXPECT_LE(entry.value_bytes, 45u * 1024u * 1024u);
    }
  }
  EXPECT_TRUE(found_resident_set_p50);
}

TEST_F(CobaltMemoryMetricsHelperTest, GetLiveMemoryBreakdownReturnsValidMetrics) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  auto live_metrics = GetLiveMemoryBreakdown();
  ASSERT_TRUE(live_metrics.has_value());
  EXPECT_FALSE(live_metrics->empty());
  bool found_rss_live = false;
  for (const auto& entry : *live_metrics) {
    EXPECT_GT(entry.value_bytes, 0u);
    EXPECT_TRUE(entry.name.ends_with(kLiveSuffix));
    if (entry.name ==
        base::StrCat({"Memory.Browser.ResidentSet", kLiveSuffix})) {
      found_rss_live = true;
    }
  }
  EXPECT_TRUE(found_rss_live);
#else
  auto live_metrics = GetLiveMemoryBreakdown();
  EXPECT_FALSE(live_metrics.has_value());
#endif
}

}  // namespace blink
