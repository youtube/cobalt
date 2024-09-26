// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_browser_observer.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests FullscreenBrowserObserver functionality.
class FullscreenBrowserObserverTest : public PlatformTest {
 public:
  FullscreenBrowserObserverTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    model_ = std::make_unique<FullscreenModel>();
    controller_ = std::make_unique<TestFullscreenController>(model_.get());
    mediator_ =
        std::make_unique<FullscreenMediator>(controller_.get(), model_.get());
    web_state_list_observer_ = std::make_unique<FullscreenWebStateListObserver>(
        controller_.get(), model_.get(), mediator_.get());
    browser_observer_ = std::make_unique<FullscreenBrowserObserver>(
        web_state_list_observer_.get(), browser_.get());
    web_state_list_observer_->SetWebStateList(
        browser_.get()->GetWebStateList());
  }

  ~FullscreenBrowserObserverTest() override { mediator_->Disconnect(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<FullscreenModel> model_;
  std::unique_ptr<TestFullscreenController> controller_;
  std::unique_ptr<FullscreenMediator> mediator_;
  std::unique_ptr<FullscreenWebStateListObserver> web_state_list_observer_;
  std::unique_ptr<FullscreenBrowserObserver> browser_observer_;
};

// Tests that FullscreenBrowserObserver resets the FullscreenController's
// WebStateList.
TEST_F(FullscreenBrowserObserverTest, BrowserDestroyed) {
  EXPECT_TRUE(web_state_list_observer_->GetWebStateList());
  browser_ = nullptr;
  EXPECT_FALSE(web_state_list_observer_->GetWebStateList());
}
