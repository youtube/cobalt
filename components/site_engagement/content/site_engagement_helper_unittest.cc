// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_helper.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_metrics.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace site_engagement {

using content::NavigationSimulator;

class TestSiteEngagementServiceProvider
    : public SiteEngagementService::ServiceProvider {
 public:
  explicit TestSiteEngagementServiceProvider(
      content::BrowserContext* browser_context)
      : site_engagement_service_(browser_context) {}
  virtual ~TestSiteEngagementServiceProvider() = default;

  SiteEngagementService* GetSiteEngagementService(
      content::BrowserContext* browser_context) override {
    return &site_engagement_service_;
  }

 private:
  SiteEngagementService site_engagement_service_;
};

class SiteEngagementHelperTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SiteEngagementScore::SetParamValuesForTesting();

    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    SiteEngagementService::RegisterProfilePrefs(pref_service_.registry());

    service_provider_ =
        std::make_unique<TestSiteEngagementServiceProvider>(browser_context());

    SiteEngagementService::SetServiceProvider(service_provider_.get());
  }

  void TearDown() override {
    SiteEngagementService::ClearServiceProvider(service_provider_.get());

    content::RenderViewHostTestHarness::TearDown();
  }

  SiteEngagementService::Helper* GetHelper(content::WebContents* web_contents) {
    SiteEngagementService::Helper::CreateForWebContents(web_contents);
    SiteEngagementService::Helper* helper =
        SiteEngagementService::Helper::FromWebContents(web_contents);

    DCHECK(helper);
    return helper;
  }

  void TrackingStarted(SiteEngagementService::Helper* helper) {
    helper->input_tracker_.TrackingStarted();
    helper->media_tracker_.TrackingStarted();
  }

  // Simulate a user interaction event and handle it.
  void HandleUserInput(SiteEngagementService::Helper* helper,
                       blink::WebInputEvent::Type type) {
    std::unique_ptr<blink::WebInputEvent> event;
    switch (type) {
      case blink::WebInputEvent::Type::kRawKeyDown:
        event = std::make_unique<blink::WebKeyboardEvent>();
        break;
      case blink::WebInputEvent::Type::kGestureScrollBegin:
        event = std::make_unique<blink::WebGestureEvent>();
        break;
      case blink::WebInputEvent::Type::kMouseDown:
        event = std::make_unique<blink::WebMouseEvent>();
        break;
      case blink::WebInputEvent::Type::kTouchStart:
        event = std::make_unique<blink::WebTouchEvent>();
        break;
      default:
        NOTREACHED();
    }
    event->SetType(type);
    helper->input_tracker_.DidGetUserInteraction(*event);
  }

  // Simulate a user interaction event and handle it. Reactivates tracking
  // immediately.
  void HandleUserInputAndRestartTracking(SiteEngagementService::Helper* helper,
                                         blink::WebInputEvent::Type type) {
    HandleUserInput(helper, type);
    helper->input_tracker_.TrackingStarted();
  }

  void HandleMediaPlaying(SiteEngagementService::Helper* helper,
                          bool is_hidden) {
    helper->RecordMediaPlaying(is_hidden);
  }

  void MediaStartedPlaying(SiteEngagementService::Helper* helper) {
    helper->media_tracker_.MediaStartedPlaying(
        content::WebContentsObserver::MediaPlayerInfo(false, false),
        content::MediaPlayerId::CreateMediaPlayerIdForTests());
  }

  void MediaStoppedPlaying(SiteEngagementService::Helper* helper) {
    helper->media_tracker_.MediaStoppedPlaying(
        content::WebContentsObserver::MediaPlayerInfo(false, false),
        content::MediaPlayerId::CreateMediaPlayerIdForTests(),
        content::WebContentsObserver::MediaStoppedReason::kUnspecified);
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementService::Helper* helper,
                                 std::unique_ptr<base::OneShotTimer> timer) {
    helper->input_tracker_.SetPauseTimerForTesting(std::move(timer));
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetMediaTrackerPauseTimer(SiteEngagementService::Helper* helper,
                                 std::unique_ptr<base::OneShotTimer> timer) {
    helper->media_tracker_.SetPauseTimerForTesting(std::move(timer));
  }

  bool IsTrackingInput(SiteEngagementService::Helper* helper) {
    return helper->input_tracker_.is_tracking();
  }

  void UserInputAccumulation(const blink::WebInputEvent::Type type) {
    GURL url1("https://www.google.com/");
    GURL url2("http://www.google.com/");
    content::WebContents* contents = web_contents();

    SiteEngagementService::Helper* helper = GetHelper(contents);
    SiteEngagementService* service =
        SiteEngagementService::Get(browser_context());
    DCHECK(service);

    // Check that navigation triggers engagement.
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
    TrackingStarted(helper);

    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate a user input trigger and ensure it is treated correctly.
    HandleUserInputAndRestartTracking(helper, type);

    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate three inputs , and ensure they are treated correctly.
    HandleUserInputAndRestartTracking(helper, type);
    HandleUserInputAndRestartTracking(helper, type);
    HandleUserInputAndRestartTracking(helper, type);

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));

    // Simulate inputs for a different link.
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
    TrackingStarted(helper);

    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
    EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

    HandleUserInputAndRestartTracking(helper, type);
    EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
    EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
  }

 private:
  std::unique_ptr<TestSiteEngagementServiceProvider> service_provider_;
  TestingPrefServiceSimple pref_service_;
  permissions::TestPermissionsClient permissions_client_;
};

TEST_F(SiteEngagementHelperTest, KeyPressEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::Type::kRawKeyDown);
}

TEST_F(SiteEngagementHelperTest, MouseDownEventEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::Type::kMouseDown);
}

TEST_F(SiteEngagementHelperTest, ScrollEventEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::Type::kGestureScrollBegin);
}

TEST_F(SiteEngagementHelperTest, TouchEngagementAccumulation) {
  UserInputAccumulation(blink::WebInputEvent::Type::kTouchStart);
}

TEST_F(SiteEngagementHelperTest, MediaEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  SiteEngagementService::Helper* helper = GetHelper(contents);
  SiteEngagementService* service =
      SiteEngagementService::Get(browser_context());
  DCHECK(service);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate a foreground media input and ensure it is treated correctly.
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate continual media playing, and ensure it is treated correctly.
  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.58, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate backgrounding the media.
  HandleMediaPlaying(helper, true);
  HandleMediaPlaying(helper, true);

  EXPECT_DOUBLE_EQ(0.60, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate inputs for a different link.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.1, service->GetTotalEngagementPoints());

  HandleMediaPlaying(helper, false);
  HandleMediaPlaying(helper, false);
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.54, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());
}

TEST_F(SiteEngagementHelperTest, MediaEngagement) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockOneShotTimer* media_tracker_timer = new base::MockOneShotTimer();
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));
  SiteEngagementService* service =
      SiteEngagementService::Get(browser_context());
  DCHECK(service);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  MediaStartedPlaying(helper);

  EXPECT_DOUBLE_EQ(0.50, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStoppedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.53, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.5, service->GetScore(url2));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.52, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  contents->WasHidden();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  MediaStoppedPlaying(helper);
  contents->WasShown();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0.53, service->GetScore(url2));
  EXPECT_TRUE(media_tracker_timer->IsRunning());
}

TEST_F(SiteEngagementHelperTest, MixedInputEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  SiteEngagementService::Helper* helper = GetHelper(contents);
  SiteEngagementService* service =
      SiteEngagementService::Get(browser_context());
  DCHECK(service);

  base::HistogramTester histograms;

  // Histograms should start off empty.
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              0);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kNavigation, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kFirstDailyEngagement, 1);

  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kRawKeyDown);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kTouchStart);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kTouchStart);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kRawKeyDown);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kMouseDown);

  EXPECT_DOUBLE_EQ(0.75, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              7);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kNavigation, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kKeypress, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kMouse, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kTouchGesture, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kFirstDailyEngagement, 1);

  HandleUserInputAndRestartTracking(
      helper, blink::WebInputEvent::Type::kGestureScrollBegin);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kMouseDown);
  HandleMediaPlaying(helper, true);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kTouchStart);
  HandleMediaPlaying(helper, false);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              12);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kMouse, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kScroll, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kTouchGesture, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kMediaVisible, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kMediaHidden, 1);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kFirstDailyEngagement, 1);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  TrackingStarted(helper);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.43, service->GetTotalEngagementPoints());

  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kTouchStart);
  HandleUserInputAndRestartTracking(helper,
                                    blink::WebInputEvent::Type::kRawKeyDown);

  EXPECT_DOUBLE_EQ(0.93, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.53, service->GetTotalEngagementPoints());
  histograms.ExpectTotalCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                              16);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kNavigation, 2);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kKeypress, 3);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kTouchGesture, 4);
  histograms.ExpectBucketCount(SiteEngagementMetrics::kEngagementTypeHistogram,
                               EngagementType::kFirstDailyEngagement, 2);
}

TEST_F(SiteEngagementHelperTest, CheckTimerAndCallbacks) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockOneShotTimer* input_tracker_timer = new base::MockOneShotTimer;
  base::MockOneShotTimer* media_tracker_timer = new base::MockOneShotTimer;
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));

  SiteEngagementService* service =
      SiteEngagementService::Get(browser_context());
  DCHECK(service);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should be running for navigation delay, but media timer is
  // inactive.
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  // Media timer starts once media is detected as playing.
  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.52, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Input timer should start running again after input, but the media timer
  // keeps running.
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  HandleUserInput(helper, blink::WebInputEvent::Type::kRawKeyDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.57, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Timer should start running again after input.
  HandleUserInput(helper, blink::WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.62, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  media_tracker_timer->Fire();
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should be running for navigation delay. Media is disabled again.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.14, service->GetTotalEngagementPoints());

  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  HandleUserInput(helper, blink::WebInputEvent::Type::kMouseDown);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
  EXPECT_FALSE(media_tracker_timer->IsRunning());

  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.19, service->GetTotalEngagementPoints());

  EXPECT_TRUE(media_tracker_timer->IsRunning());
  media_tracker_timer->Fire();
  EXPECT_DOUBLE_EQ(0.64, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.57, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.21, service->GetTotalEngagementPoints());
}

// Ensure that navigation and tab activation/hiding does not trigger input
// tracking until after a delay. We must manually call WasShown/WasHidden as
// they are not triggered automatically in this test environment.
TEST_F(SiteEngagementHelperTest, ShowAndHide) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* contents = web_contents();

  base::MockOneShotTimer* input_tracker_timer = new base::MockOneShotTimer();
  base::MockOneShotTimer* media_tracker_timer = new base::MockOneShotTimer();
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));
  SetMediaTrackerPauseTimer(helper, base::WrapUnique(media_tracker_timer));

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  input_tracker_timer->Fire();

  // Hiding the tab should stop input tracking. Media tracking remains inactive.
  contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Showing the tab should start tracking again after another delay. Media
  // tracking remains inactive.
  contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Start media tracking.
  MediaStartedPlaying(helper);
  EXPECT_TRUE(media_tracker_timer->IsRunning());

  // Hiding the tab should stop input tracking, but not media tracking.
  contents->WasHidden();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Showing the tab should start tracking again after another delay. Media
  // tracking continues.
  contents->WasShown();
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  input_tracker_timer->Fire();
  media_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(media_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));
}

// Verify that the site engagement helper:
// - Doesn't reset input tracking on a visible <-> occluded transition.
// - Handles a hidden <-> occluded transition like a hidden <-> visible
//   transition.
TEST_F(SiteEngagementHelperTest, Occlusion) {
  base::MockOneShotTimer* input_tracker_timer = new base::MockOneShotTimer();
  SiteEngagementService::Helper* helper = GetHelper(web_contents());
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.google.com/"));
  input_tracker_timer->Fire();

  // Visible -> Occluded transition should not affect input tracking.
  EXPECT_EQ(content::Visibility::VISIBLE, web_contents()->GetVisibility());
  web_contents()->WasOccluded();
  EXPECT_EQ(content::Visibility::OCCLUDED, web_contents()->GetVisibility());
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  // Occluded -> Visible transition should not affect input tracking.
  EXPECT_EQ(content::Visibility::OCCLUDED, web_contents()->GetVisibility());
  web_contents()->WasShown();
  EXPECT_EQ(content::Visibility::VISIBLE, web_contents()->GetVisibility());
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  // Visible -> Occluded transition should not affect input tracking.
  EXPECT_EQ(content::Visibility::VISIBLE, web_contents()->GetVisibility());
  web_contents()->WasOccluded();
  EXPECT_EQ(content::Visibility::OCCLUDED, web_contents()->GetVisibility());
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  // Occluded -> Hidden transition should stop input tracking.
  EXPECT_EQ(content::Visibility::OCCLUDED, web_contents()->GetVisibility());
  web_contents()->WasHidden();
  EXPECT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Hidden -> Occluded transition should start a timer to track input.
  EXPECT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());
  web_contents()->WasOccluded();
  EXPECT_EQ(content::Visibility::OCCLUDED, web_contents()->GetVisibility());
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
}

// Ensure tracking behavior is correct for multiple navigations in a single tab.
TEST_F(SiteEngagementHelperTest, SingleTabNavigation) {
  GURL url1("https://www.google.com/");
  GURL url2("https://www.example.com/");
  content::WebContents* contents = web_contents();

  base::MockOneShotTimer* input_tracker_timer = new base::MockOneShotTimer();
  SiteEngagementService::Helper* helper = GetHelper(contents);
  SetInputTrackerPauseTimer(helper, base::WrapUnique(input_tracker_timer));

  // Navigation should start the initial delay timer.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // Navigating before the timer fires should simply reset the timer.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));

  // When the timer fires, callbacks are added.
  input_tracker_timer->Fire();
  EXPECT_FALSE(input_tracker_timer->IsRunning());
  EXPECT_TRUE(IsTrackingInput(helper));

  // Navigation should start the initial delay timer again.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  EXPECT_TRUE(input_tracker_timer->IsRunning());
  EXPECT_FALSE(IsTrackingInput(helper));
}

}  // namespace site_engagement
