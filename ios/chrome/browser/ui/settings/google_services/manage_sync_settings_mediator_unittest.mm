// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_central_account_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class ManageSyncSettingsMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fakeSystemIdentity_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service->SignIn(
        fakeSystemIdentity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Creates the mediator for a given sync state.
  void CreateManageSyncSettingsMediator(
      SyncSettingsAccountState initialAccountState) {
    ASSERT_FALSE(mediator_);
    ASSERT_FALSE(consumer_);
    consumer_ = [[ManageSyncSettingsTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
    [consumer_ loadModel];
    mediator_ = [[ManageSyncSettingsMediator alloc]
          initWithSyncService:sync_service_mock_
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  browser_state_.get())
        authenticationService:AuthenticationServiceFactory::GetForBrowserState(
                                  browser_state_.get())
        accountManagerService:ChromeAccountManagerServiceFactory::
                                  GetForBrowserState(browser_state_.get())
                  prefService:browser_state_->GetPrefs()
          initialAccountState:initialAccountState];
    mediator_.consumer = consumer_;
  }

  void SimulateFirstSetupSyncOnWithConsentEnabled() {
    ON_CALL(*sync_service_mock_->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, HasSyncConsent()).WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(),
            IsSyncEverythingEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    CoreAccountInfo account_info;
    account_info.email = base::SysNSStringToUTF8(fakeSystemIdentity_.userEmail);
    ON_CALL(*sync_service_mock_, GetAccountInfo())
        .WillByDefault(Return(account_info));
  }

  void SimulateFirstSetupSyncOff() {
    ON_CALL(*sync_service_mock_, HasSyncConsent()).WillByDefault(Return(false));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(),
            IsSyncEverythingEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));
    CoreAccountInfo account_info;
    account_info.email = base::SysNSStringToUTF8(fakeSystemIdentity_.userEmail);
    ON_CALL(*sync_service_mock_, GetAccountInfo())
        .WillByDefault(Return(account_info));
  }

  void SimulateFirstSetupSyncOffWithSignedInAccount() {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        syncer::kReplaceSyncPromosWithSignInPromos);
    ON_CALL(*sync_service_mock_, HasSyncConsent()).WillByDefault(Return(false));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    CoreAccountInfo account_info;
    account_info.email = base::SysNSStringToUTF8(fakeSystemIdentity_.userEmail);
    ON_CALL(*sync_service_mock_, GetAccountInfo())
        .WillByDefault(Return(account_info));
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;

  // Needed for the initialization of authentication service.
  IOSChromeScopedTestingLocalState local_state_;

  syncer::MockSyncService* sync_service_mock_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  ManageSyncSettingsMediator* mediator_ = nullptr;
  ManageSyncSettingsTableViewController* consumer_ = nullptr;

  FakeSystemIdentity* fakeSystemIdentity_ =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
};

// Tests for Advanced Settings items.

// Tests that encryption is accessible even when Sync settings have not been
// confirmed.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceSetupNotCommitted) {
  CreateManageSyncSettingsMediator(
      SyncSettingsAccountState::kAdvancedInitialSyncSetup);
  SimulateFirstSetupSyncOff();

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  EXPECT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier]);

  // Encryption item is enabled.
  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(2UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  EXPECT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  EXPECT_TRUE(encryption_item.enabled);
}

// Tests that encryption is accessible when there is a Sync error due to a
// missing passphrase, but Sync has otherwise been enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledNeedsPassphrase) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  EXPECT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  EXPECT_TRUE(encryption_item.enabled);
}

// Tests that encryption is accessible when Sync is enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceEnabledWithEncryption) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  EXPECT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  EXPECT_TRUE(encryption_item.enabled);

  EXPECT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncErrorsSectionIdentifier]);
}

// Tests that "Turn off Sync" is hidden when Sync is disabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledWithTurnOffSync) {
  CreateManageSyncSettingsMediator(
      SyncSettingsAccountState::kAdvancedInitialSyncSetup);
  SimulateFirstSetupSyncOff();

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Sign out section not added.
  EXPECT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier]);
}

// Tests that "Turn off Sync" is accessible when Sync is enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceEnabledWithTurnOffSync) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item is shown.
  NSArray* sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  EXPECT_EQ(1UL, sign_out_items.count);
}

// Tests that the policy info is shown below the "Turn off Sync" item when the
// forced sign-in policy is enabled.
TEST_F(ManageSyncSettingsMediatorTest,
       SyncServiceEnabledWithTurnOffSyncWithForcedSigninPolicy) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();

  mediator_.forcedSigninEnabled = YES;

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item is shown.
  NSArray* sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  EXPECT_EQ(1UL, sign_out_items.count);

  // The footer below "Turn off Sync" is shown.
  ListItem* footer = [mediator_.consumer.tableViewModel
      footerForSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier];
  TableViewLinkHeaderFooterItem* footerTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(footer);
  EXPECT_GT([footerTextItem.text length], 0UL);
}

// Tests that a Sync error that occurs after the user has loaded the Settings
// page once will update the full page.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceSuccessThenDisabled) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();
  syncer::SyncService::DisableReasonSet disable_reasons =
      syncer::SyncService::DisableReasonSet();
  ON_CALL(*sync_service_mock_, GetDisableReasons())
      .WillByDefault([&disable_reasons]() { return disable_reasons; });

  // Loads the Sync page once in success state.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];
  // Loads the Sync page again in disabled state.
  disable_reasons = syncer::SyncService::DisableReasonSet(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});
  [mediator_ onSyncStateChanged];

  EXPECT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];
  EXPECT_EQ(1UL, error_items.count);
}

// Tests that Sync errors display a single error message when loaded one after
// the other.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceMultipleErrors) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));
  EXPECT_CALL(*sync_service_mock_, GetDisableReasons())
      .WillOnce(Return(syncer::SyncService::DisableReasonSet()))
      .WillOnce(Return(syncer::SyncService::DisableReasonSet(
          {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));

  // Loads the Sync page once in the disabled by enterprise policy error state.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];
  // Loads the Sync page again in the needs encryption passphrase error state.
  [mediator_ onSyncStateChanged];

  EXPECT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];
  ASSERT_EQ(1UL, error_items.count);
  TableViewDetailIconItem* error_item =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(error_items[0]);
  EXPECT_NSEQ(
      error_item.detailText,
      l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC));
}

// Tests that the items are correct when a sync type list is managed.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckItemsWhenSyncTypeListHasEnabledItems) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSyncing);
  SimulateFirstSetupSyncOnWithConsentEnabled();

  // Set up a policy to disable bookmarks and passwords.
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsTypeManagedByPolicy(syncer::UserSelectableType::kBookmarks))
      .WillByDefault(Return(true));
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsTypeManagedByPolicy(syncer::UserSelectableType::kPasswords))
      .WillByDefault(Return(true));

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Check sync switches.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  for (TableViewItem* item in items) {
    if (item.type == SyncEverythingItemType) {
      EXPECT_FALSE([item isKindOfClass:[SyncSwitchItem class]]);
      continue;
    } else if (item.type == BookmarksDataTypeItemType ||
               item.type == PasswordsDataTypeItemType) {
      EXPECT_TRUE([item isKindOfClass:[TableViewInfoButtonItem class]]);
      continue;
    }
    SyncSwitchItem* switch_item =
        base::apple::ObjCCastStrict<SyncSwitchItem>(item);
    if (switch_item.type == PaymentsDataTypeItemType) {
      EXPECT_FALSE(switch_item.enabled);
    } else {
      EXPECT_TRUE(switch_item.enabled);
    }
  }
}

// Tests that account types for a signed in not syncing account are showing
// correctly.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckAccountSwitchItemsForSignedInNotSyncingAccount) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSignedIn);
  SimulateFirstSetupSyncOffWithSignedInAccount();

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get account switches.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncDataTypeSectionIdentifier];

  for (TableViewItem* item in items) {
    // Check SyncEverythingItemType does not exist for signed in not syncing
    // users.
    EXPECT_FALSE(item.type == SyncEverythingItemType);
    // Check OpenTabsDataTypeItemType does not exist as it is merged and handled
    // by Hitstory type.
    EXPECT_FALSE(item.type == OpenTabsDataTypeItemType);
  }
}

// Tests that the account details item is showing for a signed in not syncing
// account.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckAccountItemForSignedInNotSyncingAccount) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSignedIn);
  SimulateFirstSetupSyncOffWithSignedInAccount();

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get account item.
  NSArray* account_item = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:AccountSectionIdentifier];

  EXPECT_EQ(1UL, account_item.count);

  TableViewCentralAccountItem* account_details =
      base::apple::ObjCCastStrict<TableViewCentralAccountItem>(account_item[0]);

  EXPECT_EQ(account_details.type,
            SyncSettingsItemType::IdentityAccountItemType);
  EXPECT_TRUE(account_details.avatarImage);
  EXPECT_NSEQ(account_details.name, fakeSystemIdentity_.userFullName);
  EXPECT_NSEQ(account_details.email, fakeSystemIdentity_.userEmail);
}

// Tests that the sign out item exists in the SignOutSectionIdentifier for a
// signed in not syncing account along with manage accounts items.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckSignOutSectionItemsForSignedInNotSyncingAccount) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSignedIn);
  SimulateFirstSetupSyncOffWithSignedInAccount();

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SignOutSectionIdentifier];

  EXPECT_EQ(ManageGoogleAccountItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[0]).type);
  EXPECT_EQ(ManageAccountsItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[1]).type);
  EXPECT_EQ(SignOutItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[2]).type);

  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[0]).text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[1]).text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM),
      base::apple::ObjCCastStrict<TableViewTextItem>(items[2]).text);
}

// Tests that Sync errors display as a text button at the top of the page for a
// signed in not syncing account.
TEST_F(ManageSyncSettingsMediatorTest,
       TestSyncErrorsForSignedInNotSyncingAccount) {
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSignedIn);
  SimulateFirstSetupSyncOffWithSignedInAccount();
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  // Loads the account settings page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  EXPECT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];

  EXPECT_EQ(2UL, error_items.count);
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<SettingsImageDetailTextItem>(error_items[0])
          .detailText,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_MESSAGE));
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<TableViewTextItem>(error_items[1]).text,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON));
}

// Tests the account state transition on sign out.
// This test to ensure the UI does not crash on sign out because of a missing
// section in that state. Reference bug crbug.com/1456446.
TEST_F(ManageSyncSettingsMediatorTest, TestAccountStateTransitionOnSignOut) {
  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator(SyncSettingsAccountState::kSignedIn);
  SimulateFirstSetupSyncOffWithSignedInAccount();

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Verify the sign out section exists.
  ASSERT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier]);
  // Verify the number of section shown in the kSignedIn state.
  ASSERT_EQ(4, [mediator_.consumer.tableViewModel numberOfSections]);

  // Set sign out expectation with empty account info.
  ON_CALL(*sync_service_mock_, GetAccountInfo())
      .WillByDefault(Return(CoreAccountInfo()));

  // Sign out.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  authentication_service->SignOut(signin_metrics::ProfileSignout::kTest,
                                  /*force_clear_browsing_data=*/true, nil);

  // Reload the Sync page.
  [mediator_ onSyncStateChanged];

  // Expected sections from the previous kSignedIn state should be showing and
  // no new sections are added in the kSignedOut state.
  EXPECT_EQ(4, [mediator_.consumer.tableViewModel numberOfSections]);
}
