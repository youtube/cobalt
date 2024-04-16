// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/format_support_query_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {

constexpr char kUmaPrefix[] = "Cobalt.Media.";

class FormatSupportQueryMetricsTest : public ::testing::Test {
 protected:
  FormatSupportQueryMetricsTest() : metrics_(&clock_) {}

  void SetUp() override { clock_.SetNowTicks(base::TimeTicks()); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;

  FormatSupportQueryMetrics metrics_;
};

TEST_F(FormatSupportQueryMetricsTest,
       ReportsMediaSourceIsTypeSupportedLatencyToUMA) {
  clock_.Advance(base::TimeDelta::FromMicroseconds(50));

  metrics_.RecordAndLogQuery(
      FormatSupportQueryAction::MEDIA_SOURCE_IS_TYPE_SUPPORTED, "", "",
      kSbMediaSupportTypeNotSupported);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "MediaSource.IsTypeSupported.Timing", 50, 1);
}

TEST_F(FormatSupportQueryMetricsTest,
       ReportsHTMLMediaElementCanPlayTypeLatencyToUMA) {
  clock_.Advance(base::TimeDelta::FromMicroseconds(50));

  metrics_.RecordAndLogQuery(
      FormatSupportQueryAction::HTML_MEDIA_ELEMENT_CAN_PLAY_TYPE, "", "",
      kSbMediaSupportTypeNotSupported);

  auto counts = histogram_tester_.GetTotalCountsForPrefix(kUmaPrefix);
  EXPECT_EQ(1, counts.size());

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "HTMLMediaElement.CanPlayType.Timing", 50, 1);
}

}  // namespace
}  // namespace media
}  // namespace cobalt
