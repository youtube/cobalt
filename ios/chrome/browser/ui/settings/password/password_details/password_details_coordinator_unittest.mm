// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::HistogramTester;

namespace {

// Creates an affiliated group with one credential.
password_manager::AffiliatedGroup GetTestAffiliatedGroup() {
  password_manager::PasswordForm form;
  form.url = GURL("https://example.com");
  form.username_value = u"user";
  form.password_value = u"password";
  password_manager::CredentialUIEntry credential(form);
  return password_manager::AffiliatedGroup(
      /*credentials=*/{credential},
      /*branding=*/password_manager::FacetBrandingInfo());
}

// Registers a mock command handler in the dispatcher.
void HandleCommand(Protocol* command_protocol, CommandDispatcher* dispatcher) {
  id mocked_handler = OCMStrictProtocolMock(command_protocol);
  [dispatcher startDispatchingToTarget:mocked_handler
                           forProtocol:command_protocol];
}

// Verifies that a given number of password details visits have been recorded.
void CheckPasswordDetailsVisitMetricsCount(
    int count,
    const HistogramTester& histogram_tester) {
  histogram_tester.ExpectUniqueSample(
      /*name=*/"PasswordManager.iOS.PasswordDetailsVisit", /*sample=*/true,
      /*count=*/count);
}

}  // namespace

// Test fixture for testing the PasswordDetailsCoordinatorTest class.
class PasswordDetailsCoordinatorTest : public PlatformTest {
 protected:
  PasswordDetailsCoordinatorTest() {
    TestChromeBrowserState::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    // Mock ApplicationCommands. Since ApplicationCommands conforms to
    // ApplicationSettingsCommands, it must be mocked as well.
    HandleCommand(@protocol(ApplicationCommands), dispatcher);
    HandleCommand(@protocol(ApplicationSettingsCommands), dispatcher);

    // Mock SnackbarCommands.
    HandleCommand(@protocol(SnackbarCommands), dispatcher);

    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after requested by the
    // coordinator. Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldReturnSynchronously = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    UINavigationController* navigation_controller =
        [[UINavigationController alloc] init];
    coordinator_ = [[PasswordDetailsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller
                                 browser:browser_.get()
                         affiliatedGroup:GetTestAffiliatedGroup()
                            reauthModule:mock_reauth_module_
                                 context:DetailsContext::kPasswordSettings];
  }

  ~PasswordDetailsCoordinatorTest() override { [coordinator_ stop]; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  MockReauthenticationModule* mock_reauth_module_;
  SceneState* scene_state_;
  PasswordDetailsCoordinator* coordinator_;
};

#pragma mark - Tests

// Tests that OnPasswordCopied will dispatch `CredentialProviderPromoCommands`.
TEST_F(PasswordDetailsCoordinatorTest, OnPasswordCopiedTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_copied", "true"}});

  // Register the command handler for `CredentialProviderPromoCommands`
  id credential_provider_promo_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:credential_provider_promo_commands_handler_mock
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  // Expect the call with correct trigger type.
  [[credential_provider_promo_commands_handler_mock expect]
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 PasswordCopied];

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onPasswordCopiedByUser];

  // Verify.
  [credential_provider_promo_commands_handler_mock verify];
}

// Tests that OnPasswordCopied will not dispatch
// `CredentialProviderPromoCommands`.
TEST_F(PasswordDetailsCoordinatorTest,
       OnPasswordCopiedTestCredentialProviderPromoDisabled) {
  base::test::ScopedFeatureList feature_list;
  // Enable another arm that will not lead to dispatching the
  // CredentialProviderPromoCommands.
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_saved", "true"}});

  // Register the command handler for `CredentialProviderPromoCommands`
  // Use `OCMStrictProtocolMock` and do not expect any so that an exception will
  // be raised when there is any invocation.
  id credential_provider_promo_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:credential_provider_promo_commands_handler_mock
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onPasswordCopiedByUser];
}

// Tests Password Visit metrics are logged only once after opening the surface.
TEST_F(PasswordDetailsCoordinatorTest, VisitMetricsAreLoggedOnlyOnce) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordAuthOnEntryV2);

  HistogramTester histogram_tester;
  CheckPasswordDetailsVisitMetricsCount(0, histogram_tester);

  // Starting the coordinator should record a visit.
  [coordinator_ start];
  CheckPasswordDetailsVisitMetricsCount(1, histogram_tester);

  // Simulate scene transitioning to the background and back to foreground. This
  // should trigger an auth request.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Simulate successful auth.
  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Validate no new visits were logged.
  CheckPasswordDetailsVisitMetricsCount(1, histogram_tester);
}

// Tests that onShareButtonPressed will result in metrics logged.
TEST_F(PasswordDetailsCoordinatorTest, OnShareButtonPressedMetricsLogged) {
  base::HistogramTester histogram_tester;

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onShareButtonPressed];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kPasswordDetailsShareButtonClicked, 1);
}
