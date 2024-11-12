// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using password_manager::PasswordCheckReferrer;

namespace password_manager {

// Test fixture for PasswordCheckupCoordinator.
class PasswordCheckupCoordinatorTest
    : public PlatformTest,
      public testing::WithParamInterface<PasswordCheckReferrer> {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    // Add test password store and affiliation service. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<affiliations::FakeAffiliationService>());
        })));

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    // Mock ApplicationCommands. Since ApplicationCommands conforms to
    // SettingsCommands, it must be mocked as well.
    mocked_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    id mocked_application_settings_command_handler =
        OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_settings_command_handler
                     forProtocol:@protocol(SettingsCommands)];

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldSkipReAuth = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    push_notification_service_ = ios::provider::CreatePushNotificationService();

    coordinator_ = [[PasswordCheckupCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()
                            reauthModule:mock_reauth_module_
                                referrer:GetParam()];

    scoped_window_.Get().rootViewController = base_navigation_controller_;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Verifies that the PasswordCheckupViewController was pushed in the
  // navigation controller.
  void CheckPasswordCheckupIsPresented() {
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  // Verifies that the PasswordCheckupViewController is not on top of the
  // navigation controller.
  void CheckPasswordCheckupIsNotPresented() {
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  // Verifies that a given number of password checkup visits have been recorded.
  void CheckPasswordCheckupVisitMetricsCount(int count) {
    histogram_tester_.ExpectUniqueSample(
        /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
        /*sample=*/password_manager::PasswordManagerSurface::kPasswordCheckup,
        /*count=*/count);
  }

  web::WebTaskEnvironment task_environment_;
  SceneState* scene_state_;
  std::unique_ptr<PushNotificationService> push_notification_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  id mocked_application_commands_handler_;
  base::HistogramTester histogram_tester_;
  PasswordCheckupCoordinator* coordinator_ = nil;
};

// Test fixture for tests opening Password Checkup without authentication
// required.
class PasswordCheckupCoordinatorWithoutReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Test fixture for tests opening Password Checkup with authentication required.
class PasswordCheckupCoordinatorWithReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Tests that Password Checkup is presented without authentication required.
TEST_P(PasswordCheckupCoordinatorWithoutReauthenticationTest,
       PasswordCheckupPresentedWithoutAuth) {
  CheckPasswordCheckupIsPresented();

  // One visit should have been recorded.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Tests that Password Check is presented only after passing authentication
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordCheckupPresentedWithAuth) {
  // Checkup should be covered until auth is passed.
  CheckPasswordCheckupIsNotPresented();

  // No visits recorded until successful auth.
  CheckPasswordCheckupVisitMetricsCount(0);

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Checkup visible.
  CheckPasswordCheckupIsPresented();

  // One visit should have been recorded.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Verifies that Password Checkup visits are logged only once after the first
// successful authentication.
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordCheckupVisitRecordedOnlyOnce) {
  CheckPasswordCheckupVisitMetricsCount(0);

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should record a visit
  CheckPasswordCheckupVisitMetricsCount(1);

  // Simulate scene transitioning to the background and back to foreground. This
  // should trigger an auth request.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Simulate successful auth.
  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Validate no new visits were logged.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Test Password Checkup entry points that do not require authentication.
INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instantiation name.
    PasswordCheckupCoordinatorWithoutReauthenticationTest,
    ::testing::Values(PasswordCheckReferrer::kPasswordSettings));

// Test Password Checkup entry points that require authentication.
INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instantiation name.
    PasswordCheckupCoordinatorWithReauthenticationTest,
    ::testing::Values(PasswordCheckReferrer::kPhishGuardDialog,
                      PasswordCheckReferrer::kSafetyCheck,
                      PasswordCheckReferrer::kPasswordBreachDialog,
                      PasswordCheckReferrer::kMoreToFixBubble,
                      PasswordCheckReferrer::kSafetyCheckMagicStack));

}  // namespace password_manager
