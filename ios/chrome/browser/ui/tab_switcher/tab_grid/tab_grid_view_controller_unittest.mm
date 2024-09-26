// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Fake WebStateList delegate that attaches the required tab helper.
class TabGridFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabGridFakeWebStateListDelegate() {}
  ~TabGridFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
  }
};

class TabGridViewControllerTest : public PlatformTest {
 protected:
  TabGridViewControllerTest() {
    view_controller_ = [[TabGridViewController alloc]
        initWithPageConfiguration:TabGridPageConfiguration::kAllPagesEnabled];

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(),
        std::make_unique<TabGridFakeWebStateListDelegate>());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  }
  ~TabGridViewControllerTest() override {}

  // Checks that `view_controller_` can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return [view_controller_ canPerformAction:NSSelectorFromString(action)
                                   withSender:sender];
  }

  // Checks that `view_controller_` can perform the `action`. The sender is set
  // to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  void ExpectUMA(NSString* action, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [view_controller_ performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  base::UserActionTester user_action_tester_;
  TabGridViewController* view_controller_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Checks that TabGridViewController returns key commands.
TEST_F(TabGridViewControllerTest, ReturnsKeyCommands) {
  EXPECT_GT(view_controller_.keyCommands.count, 0u);
}

// Checks whether TabGridViewController can perform the actions to open tabs.
TEST_F(TabGridViewControllerTest, CanPerform_OpenTabsActions) {
  NSArray<NSString*>* actions = @[
    @"keyCommand_openNewTab",
    @"keyCommand_openNewRegularTab",
    @"keyCommand_openNewIncognitoTab",
  ];

  [view_controller_ setCurrentPageAndPageControl:TabGridPageIncognitoTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageRegularTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageRemoteTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageRegularTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }
}

// Checks that TabGridViewController implements the following actions.
TEST_F(TabGridViewControllerTest, ImplementsActions) {
  // Load the view.
  std::ignore = view_controller_.view;
  [view_controller_ keyCommand_openNewTab];
  [view_controller_ keyCommand_openNewRegularTab];
  [view_controller_ keyCommand_openNewIncognitoTab];
  [view_controller_ keyCommand_find];
  [view_controller_ keyCommand_closeAll];
  [view_controller_ keyCommand_undo];
  [view_controller_ keyCommand_close];
}

// Checks that metrics are correctly reported.
TEST_F(TabGridViewControllerTest, Metrics) {
  // Load the view.
  std::ignore = view_controller_.view;
  ExpectUMA(@"keyCommand_openNewTab", "MobileKeyCommandOpenNewTab");
  ExpectUMA(@"keyCommand_openNewRegularTab",
            "MobileKeyCommandOpenNewRegularTab");
  ExpectUMA(@"keyCommand_openNewIncognitoTab",
            "MobileKeyCommandOpenNewIncognitoTab");
  ExpectUMA(@"keyCommand_find", "MobileKeyCommandSearchTabs");
  ExpectUMA(@"keyCommand_closeAll", "MobileKeyCommandCloseAll");
  ExpectUMA(@"keyCommand_undo", "MobileKeyCommandUndo");
  ExpectUMA(@"keyCommand_close", "MobileKeyCommandClose");
}

// This test ensure 2 things:
// * the key command find is available when the tab grid is currently visible,
// * the key command associated title is correct.
TEST_F(TabGridViewControllerTest, ValidateCommand_find) {
  view_controller_ = [[TabGridViewController alloc]
      initWithPageConfiguration:TabGridPageConfiguration::kIncognitoPageOnly];
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));

  [view_controller_ contentWillAppearAnimated:NO];

  EXPECT_TRUE(CanPerform(@"keyCommand_find"));
  id findTarget = [view_controller_ targetForAction:@selector(keyCommand_find)
                                         withSender:nil];
  EXPECT_EQ(findTarget, view_controller_);

  // Ensures that the title is correct.
  for (UIKeyCommand* command in view_controller_.keyCommands) {
    [view_controller_ validateCommand:command];
    if (command.action == @selector(keyCommand_find)) {
      EXPECT_TRUE([command.discoverabilityTitle
          isEqualToString:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_KEYBOARD_SEARCH_TABS)]);
    }
  }
}

// Checks when Close All and Undo keyboard shortcuts are possible.
TEST_F(TabGridViewControllerTest, CanPerform_CloseAllAndUndo) {
  view_controller_ = [[TabGridViewController alloc]
      initWithPageConfiguration:TabGridPageConfiguration::kIncognitoPageOnly];
  EXPECT_FALSE(CanPerform(@"keyCommand_closeAll"));
  EXPECT_FALSE(CanPerform(@"keyCommand_undo"));

  [view_controller_ contentWillAppearAnimated:NO];

  EXPECT_FALSE(CanPerform(@"keyCommand_closeAll"));
  EXPECT_FALSE(CanPerform(@"keyCommand_undo"));
  TabGridMediator* incognitoMediator = [[TabGridMediator alloc]
      initWithConsumer:view_controller_.incognitoTabsConsumer];
  [incognitoMediator setBrowser:browser_.get()];
  view_controller_.incognitoTabsDelegate = incognitoMediator;
  [view_controller_.incognitoTabsDelegate addNewItem];
  EXPECT_TRUE(CanPerform(@"keyCommand_closeAll"));
  EXPECT_FALSE(CanPerform(@"keyCommand_undo"));
  [view_controller_.incognitoTabsDelegate closeAllItems];
  EXPECT_FALSE(CanPerform(@"keyCommand_closeAll"));
  EXPECT_FALSE(CanPerform(@"keyCommand_undo"));

  // Forces the TabGridMediator to removes its Observer from WebStateList
  // before the Browser is destroyed.
  incognitoMediator.browser = nullptr;
  incognitoMediator = nil;
}

// Checks that the ESC keyboard shortcut is always possible.
TEST_F(TabGridViewControllerTest, CanPerform_Close) {
  EXPECT_TRUE(CanPerform(@"keyCommand_close"));
}

}  // namespace
