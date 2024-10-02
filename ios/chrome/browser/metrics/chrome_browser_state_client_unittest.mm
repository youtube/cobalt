// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/chrome_browser_state_client.h"

#import "base/time/default_clock.h"
#import "base/time/default_tick_clock.h"
#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace metrics {

class ChromeBrowserStateClientTest : public PlatformTest {
 public:
  ChromeBrowserStateClientTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(
                TestChromeBrowserState::Builder().Build())) {}
  ~ChromeBrowserStateClientTest() override {}

  void SetUp() override { PlatformTest::SetUp(); }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
};

TEST_F(ChromeBrowserStateClientTest, GetNetworkTime) {
  // Set initial time of the network clock.
  base::TimeDelta resolution = base::Milliseconds(17);
  base::TimeDelta latency = base::Milliseconds(50);
  base::DefaultClock clock;
  base::DefaultTickClock tick_clock;
  GetApplicationContext()->GetNetworkTimeTracker()->UpdateNetworkTime(
      clock.Now() - latency / 2, resolution, latency, tick_clock.NowTicks());

  // Verify network clock gives non-null time.
  ChromeBrowserStateClient profile_client;
  EXPECT_FALSE(profile_client.GetNetworkTime().is_null());
}

TEST_F(ChromeBrowserStateClientTest, GetSyncService) {
  ChromeBrowserStateClient profile_client;
  // Verify if it possible to retrieve the instance of the SyncService
  // associated with the ChromeBrowserState.
  EXPECT_TRUE(profile_client.GetSyncService());
}

TEST_F(ChromeBrowserStateClientTest, GetNumberOfProfilesOnDisk) {
  ChromeBrowserStateClient profile_client;
  // On ChromeBrowserState was created and registered with the
  // ChromeBrowserStateManager, check the client returns the correct
  // value.
  EXPECT_EQ(1, profile_client.GetNumberOfProfilesOnDisk());
}

}  // namespace metrics
