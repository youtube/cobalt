// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace password_manager {

// Test fixture for PasswordIssuesCoordinator.
class PasswordIssuesCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kIOSPasswordAuthOnEntryV2);

    TestChromeBrowserState::Builder builder;
    // Add test password store. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Keep a scoped reference to IOSChromePasswordCheckManager until the test
    // finishes, otherwise it gets destroyed as soon as PasswordIssuesMediator
    // init goes out of scope.
    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForBrowserState(
            browser_state_.get());

    // Mock ApplicationCommands. Since ApplicationCommands conforms to
    // ApplicationSettingsCommands, it must be mocked as well.
    mocked_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    id mocked_application_settings_command_handler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_settings_command_handler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldReturnSynchronously = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    coordinator_ = [[PasswordIssuesCoordinator alloc]
              initForWarningType:password_manager::WarningType::
                                     kCompromisedPasswordsWarning
        baseNavigationController:base_navigation_controller_
                         browser:browser_.get()];

    coordinator_.reauthModule = mock_reauth_module_;

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    scoped_window_.Get().rootViewController = base_navigation_controller_;
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Starts the coordinator.
  //  - skip_auth_on_start: Whether to skip local authentication when the
  //  coordinator is started.
  void StartCoordinatorSkippingAuth(BOOL skip_auth_on_start) {
    coordinator_.skipAuthenticationOnStart = skip_auth_on_start;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  // Verifies that the PasswordIssuesTableViewController was pushed in the
  // navigation controller.
  void CheckPasswordIssuesIsPresented() {
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordIssuesTableViewController class]]);
  }

  // Verifies that the PasswordIssuesTableViewController is not on top of the
  // navigation controller.
  void CheckPasswordIssuesIsNotPresented() {
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordIssuesTableViewController class]]);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  // IOSChromePasswordCheckManager must be destroyed before browser otherwise it
  // crashes when unregistering itself as an observer of a keyed service.
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  SceneState* scene_state_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mocked_application_commands_handler_;
  PasswordIssuesCoordinator* coordinator_ = nil;
};

// Tests that Password Issues is presented without authentication required.
TEST_F(PasswordIssuesCoordinatorTest, PasswordIssuesPresentedWithoutAuth) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/YES);

  CheckPasswordIssuesIsPresented();
}

// Tests that Password Issues is presented only after passing authentication
TEST_F(PasswordIssuesCoordinatorTest, PasswordIssuesPresentedWithAuth) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/NO);

  // Password Issues should be covered until auth is passed.
  CheckPasswordIssuesIsNotPresented();

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Password Issues visible.
  CheckPasswordIssuesIsPresented();
}

}  // namespace password_manager
