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

#include "cobalt/dom/source_buffer_metrics.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {
constexpr char kUmaPrefix[] = "Cobalt.Media.SourceBuffer.";

class SourceBufferMetricsTest : public ::testing::Test {
 protected:
  SourceBufferMetricsTest()
      : metrics_(std::make_unique<SourceBufferMetrics>(
            /*is_primary_video=*/true, &clock_)) {}

  void SetUp() override { clock_.SetNowTicks(base::TimeTicks()); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;

  std::unique_ptr<SourceBufferMetrics> metrics_;
};

TEST_F(SourceBufferMetricsTest, RecordsPrepareAppendTelemetry) {
  metrics_->StartTracking(SourceBufferMetricsAction::PREPARE_APPEND);

  clock_.Advance(base::TimeDelta::FromMicroseconds(100));
  metrics_->EndTracking(SourceBufferMetricsAction::PREPARE_APPEND, 1234);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "PrepareAppend.Timing", 100, 1);
}

TEST_F(SourceBufferMetricsTest, RecordsAppendBufferTelemetry) {
  metrics_->StartTracking(SourceBufferMetricsAction::APPEND_BUFFER);

  clock_.Advance(base::TimeDelta::FromMicroseconds(100));
  metrics_->EndTracking(SourceBufferMetricsAction::APPEND_BUFFER, 1234);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "AppendBuffer.Timing", 100, 1);
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
