// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using VerdictCacheManagerFactoryTest = PlatformTest;

// Checks that VerdictCacheManagerFactory returns different instances
// for an off-the-record browser state and a regular browser state.
TEST_F(VerdictCacheManagerFactoryTest, OffTheRecordUsesDifferentInstance) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();

  // There should be a non-null instance for an off-the-record browser state.
  EXPECT_TRUE(VerdictCacheManagerFactory::GetForBrowserState(
      browser_state->GetOffTheRecordChromeBrowserState()));

  EXPECT_NE(VerdictCacheManagerFactory::GetForBrowserState(browser_state.get()),
            VerdictCacheManagerFactory::GetForBrowserState(
                browser_state->GetOffTheRecordChromeBrowserState()));
}
