// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_action_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class FamilyPromoCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_ = std::make_unique<TestBrowser>(
        TestChromeBrowserState::Builder().Build().get());

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
  }

  FamilyPromoCoordinator* CreateFamilyPromoCoordinator(FamilyPromoType type) {
    return [[FamilyPromoCoordinator alloc]
        initWithFamilyPromoType:type
             baseViewController:[[FakeUIViewController alloc] init]
                        browser:browser_.get()];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;

  id mock_application_commands_handler_;
  id mock_application_settings_commands_handler_;
};

TEST_F(FamilyPromoCoordinatorTest,
       OpensFamilyCreationSiteOnCreateFamilyLinkTap) {
  base::HistogramTester histogram_tester;

  FamilyPromoCoordinator* coordinator =
      CreateFamilyPromoCoordinator(FamilyPromoType::kUserNotInFamilyGroup);
  [coordinator start];
  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(FamilyPromoActionHandler)]);

  OCMExpect([mock_application_commands_handler_
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                            OpenNewTabCommand* command) {
        return command.URL == GURL("https://families.google.com/families/");
      }]]);
  [(id<FamilyPromoActionHandler>)coordinator createFamilyGroupLinkWasTapped];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kFamilyPromoCreateFamilyGroupClicked, 1);
}

TEST_F(FamilyPromoCoordinatorTest,
       OpensFamilyManagementSiteOnInviteMembersLinkTap) {
  base::HistogramTester histogram_tester;

  FamilyPromoCoordinator* coordinator = CreateFamilyPromoCoordinator(
      FamilyPromoType::kUserWithNoOtherFamilyMembers);
  [coordinator start];
  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(FamilyPromoActionHandler)]);

  OCMExpect([mock_application_commands_handler_
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                            OpenNewTabCommand* command) {
        return command.URL == GURL("https://families.google.com/families/");
      }]]);
  [(id<FamilyPromoActionHandler>)coordinator createFamilyGroupLinkWasTapped];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kFamilyPromoInviteFamilyMembersClicked, 1);
}

TEST_F(FamilyPromoCoordinatorTest, LogsMetricOnFamilyPromoGotItClicked) {
  base::HistogramTester histogram_tester;

  FamilyPromoCoordinator* coordinator =
      CreateFamilyPromoCoordinator(FamilyPromoType::kUserNotInFamilyGroup);
  [coordinator start];
  ASSERT_TRUE([coordinator
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);

  [(id<ConfirmationAlertActionHandler>)
          coordinator confirmationAlertPrimaryAction];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kFamilyPromoGotItClicked, 1);
}
