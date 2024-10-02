// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_navigation_browser_agent.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeNTPDelegate : NSObject <WebNavigationNTPDelegate>

// Redeclare protocol property readwrite.
@property(nonatomic, readwrite, getter=isNTPActiveForCurrentWebState)
    BOOL NTPActiveForCurrentWebState;

// YES if reloadNTPForWebState was called for `webState`
- (BOOL)didReloadForWebState:(web::WebState*)webState;

@end

@implementation FakeNTPDelegate {
  web::WebState* _lastReloadedWebState;
}

- (void)reloadNTPForWebState:(web::WebState*)webState {
  _lastReloadedWebState = webState;
}

- (BOOL)didReloadForWebState:(web::WebState*)webState {
  return webState == _lastReloadedWebState;
}

@end

namespace {

class WebNavigationBrowserAgentTest : public PlatformTest {
 public:
  WebNavigationBrowserAgentTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    delegate_ = [[FakeNTPDelegate alloc] init];
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = WebNavigationBrowserAgent::FromBrowser(browser_.get());
    agent_->SetDelegate(delegate_);
    WebStateOpener opener;
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        /*index=*/0, /*webState=*/std::move(web_state),
        /*insertion_flags=*/WebStateList::InsertionFlags::INSERT_ACTIVATE,
        /*opener=*/opener);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  FakeNTPDelegate* delegate_;
  WebNavigationBrowserAgent* agent_;
  // Navigation manager for the web state at index 0 in `browser_`'s web state
  // list.
  web::FakeNavigationManager* navigation_manager_;
};

// Tests that reloading when there is no active NTP reloads the web state, and
// the NTP delegate doesn't reload.
TEST_F(WebNavigationBrowserAgentTest, TestReloadNoNTP) {
  agent_->Reload();
  EXPECT_FALSE([delegate_
      didReloadForWebState:browser_->GetWebStateList()->GetWebStateAt(0)]);
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());
}

// Tests that reloading when there is an active NTP causes the delegate to
// reload, and the web state doesn't reload.
TEST_F(WebNavigationBrowserAgentTest, TestReloadActiveNTP) {
  delegate_.NTPActiveForCurrentWebState = YES;
  agent_->Reload();
  EXPECT_TRUE([delegate_
      didReloadForWebState:browser_->GetWebStateList()->GetWebStateAt(0)]);
  EXPECT_FALSE(navigation_manager_->ReloadWasCalled());
}

}  // namespace
