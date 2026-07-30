// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ActorBrowserAgentTest : public PlatformTest {
 public:
  ActorBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_actor_overlay_handler_ =
        OCMProtocolMock(@protocol(ActorOverlayCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_actor_overlay_handler_
                     forProtocol:@protocol(ActorOverlayCommands)];

    ActorBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = ActorBrowserAgent::FromBrowser(browser_.get());
  }

  ActorBrowserAgentTest(const ActorBrowserAgentTest&) = delete;
  ActorBrowserAgentTest& operator=(const ActorBrowserAgentTest&) = delete;

  ~ActorBrowserAgentTest() override = default;

  // Appends a `WebState` with an `ActorTabHelper` to the `WebStateList`.
  web::FakeWebState* AppendActiveWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* appended_web_state = web_state.get();
    ActorTabHelper::CreateForWebState(appended_web_state);
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return appended_web_state;
  }

 protected:
  // Task environment to support asynchronous runs.
  web::WebTaskEnvironment task_environment_;
  // Profile used for testing.
  std::unique_ptr<TestProfileIOS> profile_;
  // The `TestBrowser` instance used for testing.
  std::unique_ptr<TestBrowser> browser_;
  // Mock handler used to verify dispatched `ActorOverlayCommands`.
  id<ActorOverlayCommands> mock_actor_overlay_handler_;
  // The `ActorBrowserAgent` instance under test.
  raw_ptr<ActorBrowserAgent> agent_;
};

// Test that changing the active `WebState` updates the tab helper observation,
// and triggers the hide command.
TEST_F(ActorBrowserAgentTest, ActiveWebStateChange) {
  // 1. Appending the first `WebState` (non-actuating).
  web::FakeWebState* web_state1 = AppendActiveWebState();

  // 2. Setting it to actuating.
  // Expect it to show.
  OCMExpect(
      [mock_actor_overlay_handler_ showActorOverlayForWebState:web_state1]);
  ActorTabHelper::FromWebState(web_state1)->SetActuating(true);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);

  // 3. Appending a second `WebState` (non-actuating).
  // Expect it to hide the first one's UI.
  OCMExpect([mock_actor_overlay_handler_ hideActorOverlay]);
  web::FakeWebState* web_state2 = AppendActiveWebState();
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);

  // 4. Setting the second `WebState` to actuating.
  // Expect it to show.
  OCMExpect(
      [mock_actor_overlay_handler_ showActorOverlayForWebState:web_state2]);
  ActorTabHelper::FromWebState(web_state2)->SetActuating(true);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);

  // 5. Switching back to the first `WebState` (which is actuating).
  // Expect it to hide the second UI first, then show the first UI.
  OCMExpect([mock_actor_overlay_handler_ hideActorOverlay]);
  OCMExpect(
      [mock_actor_overlay_handler_ showActorOverlayForWebState:web_state1]);
  browser_->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);
}

// Test that when the observed `ActorTabHelper` changes its actuating state, the
// browser agent dispatches the corresponding commands.
TEST_F(ActorBrowserAgentTest, TabHelperActuationChange) {
  web::FakeWebState* web_state = AppendActiveWebState();

  // Test start actuation.
  OCMExpect(
      [mock_actor_overlay_handler_ showActorOverlayForWebState:web_state]);
  ActorTabHelper::FromWebState(web_state)->SetActuating(true);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);

  // Test stop actuation.
  OCMExpect([mock_actor_overlay_handler_ hideActorOverlay]);
  ActorTabHelper::FromWebState(web_state)->SetActuating(false);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);
}

// Test that closing the active actuating `WebState` correctly hides the UI.
TEST_F(ActorBrowserAgentTest, CloseActiveWebState) {
  web::FakeWebState* web_state = AppendActiveWebState();

  // Actuate.
  OCMExpect(
      [mock_actor_overlay_handler_ showActorOverlayForWebState:web_state]);
  ActorTabHelper::FromWebState(web_state)->SetActuating(true);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);

  // Close the active `WebState`.
  OCMExpect([mock_actor_overlay_handler_ hideActorOverlay]);
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  EXPECT_OCMOCK_VERIFY(mock_actor_overlay_handler_);
}
