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

#include "cobalt/media/base/metrics_provider.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {
constexpr char kUmaPrefix[] = "Cobalt.Media.";

class MediaMetricsProviderTest : public ::testing::Test {
 protected:
  MediaMetricsProviderTest() : metrics_(&clock_) {}

  void SetUp() override { clock_.SetNowTicks(base::TimeTicks()); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;

  MediaMetricsProvider metrics_;
};

TEST_F(MediaMetricsProviderTest, ReportsSeekLatency) {
  metrics_.StartTrackingAction(WebMediaPlayerAction::SEEK);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(WebMediaPlayerAction::SEEK);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "WebMediaPlayer.Seek.Timing", 100, 1);
}

TEST_F(MediaMetricsProviderTest, SupportsTrackingMultipleActions) {
  metrics_.StartTrackingAction(WebMediaPlayerAction::SEEK);
  metrics_.StartTrackingAction(WebMediaPlayerAction::UNKNOWN_ACTION);

  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  metrics_.EndTrackingAction(WebMediaPlayerAction::UNKNOWN_ACTION);

  clock_.Advance(base::TimeDelta::FromMilliseconds(1000));
  metrics_.EndTrackingAction(WebMediaPlayerAction::SEEK);

  histogram_tester_.ExpectUniqueSample(
      std::string(kUmaPrefix) + "WebMediaPlayer.Seek.Timing", 1100, 1);
}

}  // namespace
}  // namespace media
}  // namespace cobalt
