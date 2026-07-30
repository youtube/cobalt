// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class ActorOverlayCoordinatorTest : public PlatformTest {
 public:
  ActorOverlayCoordinatorTest(const ActorOverlayCoordinatorTest&) = delete;
  ActorOverlayCoordinatorTest& operator=(const ActorOverlayCoordinatorTest&) =
      delete;

 protected:
  ActorOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    web_state_ = std::make_unique<web::FakeWebState>();
  }

  // Task environment that manages the message loops and threads for testing.
  web::WebTaskEnvironment task_environment_;
  // Profile instance used to initialize the browser.
  std::unique_ptr<TestProfileIOS> profile_;
  // Browser instance containing the active tab undergoing test.
  std::unique_ptr<TestBrowser> browser_;
  // Base view controller used to present the coordinator's UI.
  UIViewController* base_view_controller_ = nil;
  // Fake web state representing the tab being actuated.
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Test that the designated initializer successfully sets up the instance.
TEST_F(ActorOverlayCoordinatorTest, InitializerAndLifecycle) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        webState:web_state_.get()];

  EXPECT_NE(coordinator, nil);

  // Verify `start()` and `stop()` execute without failure.
  [coordinator start];
  [coordinator stop];
}

// Test that if the underlying `WebState` is destroyed while the coordinator is
// running, the coordinator can be stopped safely without crashing.
TEST_F(ActorOverlayCoordinatorTest, WebStateDestroyedBeforeStop) {
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        webState:web_state_.get()];

  EXPECT_NE(coordinator, nil);

  [coordinator start];

  // Destroy the `WebState`.
  web_state_.reset();

  // Stopping the coordinator should not crash.
  [coordinator stop];
}

}  // namespace
