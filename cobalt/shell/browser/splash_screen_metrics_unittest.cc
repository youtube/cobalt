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

#include "cobalt/shell/browser/splash_screen_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using Event = SplashScreenMetrics::Event;
using SwitchTrigger = SplashScreenMetrics::SwitchTrigger;

class SplashScreenMetricsTest : public testing::Test {
 protected:
  void SetUp() override {
    SplashScreenMetrics::ResetForTesting();
    // The launch starts at t=0.
    SplashScreenMetrics::SetAppStartTime(base::TimeTicks::Now());
  }

  void TearDown() override { SplashScreenMetrics::ResetForTesting(); }

  void AdvanceMs(int ms) {
    task_environment_.FastForwardBy(base::Milliseconds(ms));
  }

  void ExpectTimeMs(const char* name, int ms) {
    histogram_tester_.ExpectUniqueTimeSample(name, base::Milliseconds(ms), 1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(SplashScreenMetricsTest, ImageSplashScreenClosedAfterMainLoaded) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainNavigationStart);  // t=100
  metrics->MarkEvent(Event::kSplashStart);          // t=100
  AdvanceMs(200);
  metrics->MarkEvent(Event::kSplashLoaded);  // t=300
  AdvanceMs(50);
  metrics->MarkEvent(Event::kSplashFirstPaint);  // t=350
  AdvanceMs(150);
  metrics->MarkEvent(Event::kMainCommit);  // t=500
  AdvanceMs(500);
  metrics->MarkEvent(Event::kMainLoaded);  // t=1000
  AdvanceMs(300);
  metrics->MarkEvent(Event::kSplashClosed);  // t=1300
  metrics->OnSwitchedToMain(SwitchTrigger::kSplashClosed);

  ExpectTimeMs("Cobalt.Startup.Timeline.SplashStart.Image", 100);
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashLoaded.Image", 300);
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashFirstPaint.Image", 350);
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashContentShown.Image", 350);
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashContentEnded.Image", 1300);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainNavigationStart.Image", 100);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainCommit.Image", 500);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainLoaded.Image", 1000);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainVisible.Image", 1300);

  ExpectTimeMs("Cobalt.SplashScreen.Time.PageLoad.Image", 200);
  ExpectTimeMs("Cobalt.SplashScreen.Time.LoadedToContentShown.Image", 50);
  ExpectTimeMs("Cobalt.SplashScreen.Time.ContentShownToSwitch.Image", 950);
  ExpectTimeMs("Cobalt.SplashScreen.Time.MainLoadedToSwitch.Image", 300);

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.ContentEndedBeforeSwitch.Image", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.SwitchTrigger.Image", SwitchTrigger::kSplashClosed,
      1);
  EXPECT_EQ(
      histogram_tester_.GetTotalCountsForPrefix("Cobalt.SplashScreen.Time.")
          .size(),
      4u);
  histogram_tester_.ExpectTotalCount("Cobalt.SplashScreen.SwitchTrigger.Video",
                                     0);
}

TEST_F(SplashScreenMetricsTest, VideoCutOffByTimer) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kSplashStart);  // t=0
  AdvanceMs(100);
  metrics->MarkEvent(Event::kSplashLoaded);      // t=100
  metrics->MarkEvent(Event::kSplashFirstPaint);  // t=100
  AdvanceMs(200);
  metrics->MarkEvent(Event::kSplashVideoStarted);  // t=300
  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainLoaded);  // t=400
  AdvanceMs(1200);
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);  // t=1600

  // The video started playing, so it's recorded as a video splash screen and
  // the content was shown when the video started.
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashFirstPaint.Video", 100);
  ExpectTimeMs("Cobalt.Startup.Timeline.SplashContentShown.Video", 300);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Startup.Timeline.SplashContentEnded.Video", 0);
  ExpectTimeMs("Cobalt.SplashScreen.Time.LoadedToContentShown.Video", 200);
  ExpectTimeMs("Cobalt.SplashScreen.Time.ContentShownToSwitch.Video", 1300);
  ExpectTimeMs("Cobalt.SplashScreen.Time.MainLoadedToSwitch.Video", 1200);

  // The video didn't reach its end before the switch.
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.ContentEndedBeforeSwitch.Video", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.SwitchTrigger.Video", SwitchTrigger::kTimer, 1);
  histogram_tester_.ExpectTotalCount("Cobalt.SplashScreen.SwitchTrigger.Image",
                                     0);
}

TEST_F(SplashScreenMetricsTest, VideoEndedBeforeMainLoaded) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kSplashVideoStarted);  // t=0
  AdvanceMs(1000);
  metrics->MarkEvent(Event::kSplashVideoEnded);  // t=1000
  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainLoaded);  // t=1100
  metrics->OnSwitchedToMain(SwitchTrigger::kMainLoaded);

  ExpectTimeMs("Cobalt.Startup.Timeline.SplashContentEnded.Video", 1000);
  ExpectTimeMs("Cobalt.SplashScreen.Time.MainLoadedToSwitch.Video", 0);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.ContentEndedBeforeSwitch.Video", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.SwitchTrigger.Video", SwitchTrigger::kMainLoaded, 1);
}

TEST_F(SplashScreenMetricsTest, WithoutSplashScreen) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/false, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainNavigationStart);  // t=100
  AdvanceMs(400);
  metrics->MarkEvent(Event::kMainCommit);  // t=500
  AdvanceMs(300);
  metrics->MarkEvent(Event::kMainFirstPaint);  // t=800

  // Nothing is recorded until the main WebContents has also loaded.
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix("Cobalt.Startup.").empty());

  AdvanceMs(700);
  metrics->MarkEvent(Event::kMainLoaded);  // t=1500

  ExpectTimeMs("Cobalt.Startup.Timeline.MainNavigationStart.NoSplash", 100);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainCommit.NoSplash", 500);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainLoaded.NoSplash", 1500);
  // Without a splash screen, the main WebContents is visible from its first
  // paint.
  ExpectTimeMs("Cobalt.Startup.Timeline.MainVisible.NoSplash", 800);

  // There's no switch without a splash screen.
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);
  EXPECT_TRUE(histogram_tester_.GetTotalCountsForPrefix("Cobalt.SplashScreen.")
                  .empty());
}

TEST_F(SplashScreenMetricsTest, WithoutSplashScreenLoadedBeforeFirstPaint) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/false, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(500);
  metrics->MarkEvent(Event::kMainLoaded);  // t=500
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Startup.Timeline.MainLoaded.NoSplash", 0);

  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainFirstPaint);  // t=600
  ExpectTimeMs("Cobalt.Startup.Timeline.MainLoaded.NoSplash", 500);
  ExpectTimeMs("Cobalt.Startup.Timeline.MainVisible.NoSplash", 600);
}

TEST_F(SplashScreenMetricsTest, MainReadyAfterSwitch) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(1000);
  metrics->MarkEvent(Event::kMainLoaded);                 // t=1000
  metrics->OnSwitchedToMain(SwitchTrigger::kMainLoaded);  // t=1000
  histogram_tester_.ExpectTotalCount("Cobalt.Startup.Timeline.MainReady.Image",
                                     0);

  AdvanceMs(500);
  metrics->MarkEvent(Event::kMainReady);  // t=1500
  ExpectTimeMs("Cobalt.Startup.Timeline.MainReady.Image", 1500);

  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainReady);  // Ignored.
  ExpectTimeMs("Cobalt.Startup.Timeline.MainReady.Image", 1500);
}

TEST_F(SplashScreenMetricsTest, MainReadyBeforeSwitchWithVideo) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kSplashVideoStarted);  // t=0
  AdvanceMs(800);
  metrics->MarkEvent(Event::kMainReady);  // t=800

  ExpectTimeMs("Cobalt.Startup.Timeline.MainReady.Video", 800);
  histogram_tester_.ExpectTotalCount("Cobalt.Startup.Timeline.MainReady.Image",
                                     0);
}

TEST_F(SplashScreenMetricsTest, MainReadyWithoutSplashScreen) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/false, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(2000);
  metrics->MarkEvent(Event::kMainReady);  // t=2000

  ExpectTimeMs("Cobalt.Startup.Timeline.MainReady.NoSplash", 2000);
}

TEST_F(SplashScreenMetricsTest, NotRecordedWhenStartedHidden) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/false);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kSplashStart);
  metrics->MarkEvent(Event::kMainLoaded);
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);
  metrics->MarkEvent(Event::kMainReady);

  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix("Cobalt.Startup.").empty());
  EXPECT_TRUE(histogram_tester_.GetTotalCountsForPrefix("Cobalt.SplashScreen.")
                  .empty());
}

TEST_F(SplashScreenMetricsTest, NotRecordedAfterInvalidate) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/false, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kMainFirstPaint);
  metrics->Invalidate();
  metrics->MarkEvent(Event::kMainLoaded);
  metrics->MarkEvent(Event::kMainReady);

  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix("Cobalt.Startup.").empty());
}

TEST_F(SplashScreenMetricsTest, RecordedOnlyOnce) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kMainLoaded);
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);
  metrics->OnSwitchedToMain(SwitchTrigger::kSplashClosed);

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.SplashScreen.SwitchTrigger.Image", SwitchTrigger::kTimer, 1);
}

TEST_F(SplashScreenMetricsTest, OnlyFirstOccurrenceOfEventCounts) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainLoaded);  // t=100
  AdvanceMs(100);
  metrics->MarkEvent(Event::kMainLoaded);            // Ignored.
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);  // t=200

  ExpectTimeMs("Cobalt.Startup.Timeline.MainLoaded.Image", 100);
  ExpectTimeMs("Cobalt.SplashScreen.Time.MainLoadedToSwitch.Image", 100);
}

TEST_F(SplashScreenMetricsTest, OnlyFirstLaunchIsTracked) {
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  EXPECT_TRUE(metrics);
  // E.g., a second window.
  EXPECT_FALSE(SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/false, /*is_visible=*/true));
}

TEST_F(SplashScreenMetricsTest, AppStartTimeIsOnlySetOnce) {
  AdvanceMs(500);
  // Ignored, as SetUp() already set the start time at t=0.
  SplashScreenMetrics::SetAppStartTime(base::TimeTicks::Now());
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);  // t=500

  ExpectTimeMs("Cobalt.Startup.Timeline.MainVisible.Image", 500);
}

TEST_F(SplashScreenMetricsTest, NoTimelineWithoutAppStartTime) {
  // Clears the start time set in SetUp().
  SplashScreenMetrics::ResetForTesting();
  auto metrics = SplashScreenMetrics::MaybeCreateForLaunch(
      /*has_splash_screen=*/true, /*is_visible=*/true);
  ASSERT_TRUE(metrics);

  metrics->MarkEvent(Event::kSplashFirstPaint);
  AdvanceMs(100);
  metrics->OnSwitchedToMain(SwitchTrigger::kTimer);

  // The timeline needs the start time, but the intervals don't.
  EXPECT_TRUE(
      histogram_tester_.GetTotalCountsForPrefix("Cobalt.Startup.").empty());
  ExpectTimeMs("Cobalt.SplashScreen.Time.ContentShownToSwitch.Image", 100);
}

}  // namespace

}  // namespace content
