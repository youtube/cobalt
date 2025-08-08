// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/features.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_mock.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

class SignoutActionSheetCoordinatorTest : public PlatformTest {
 public:
  SignoutActionSheetCoordinatorTest() {
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void SetUp() override {
    PlatformTest::SetUp();

    identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  void TearDown() override {
    [signout_coordinator_ stop];
    signout_coordinator_ = nil;
    PlatformTest::TearDown();
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  // Sign-out coordinator.
  SignoutActionSheetCoordinator* CreateCoordinator() {
    signout_coordinator_ = [[SignoutActionSheetCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                              rect:view_controller_.view.frame
                              view:view_controller_.view
                        withSource:signin_metrics::ProfileSignout::
                                       kUserClickedSignoutSettings];
    signout_coordinator_.completion = ^(BOOL success) {
    };
    return signout_coordinator_;
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  base::test::TaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  SignoutActionSheetCoordinator* signout_coordinator_ = nullptr;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_ = nullptr;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  id<SystemIdentity> identity_ = nil;

  syncer::MockSyncService* sync_service_mock_ = nullptr;
};

// Tests that a signed-in user with Sync enabled will have an action sheet with
// a sign-out title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithSync) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(true));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_NE(nil, signout_coordinator_.title);
}

// Tests that a signed-in user with Sync disabled will have an action sheet with
// no title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithoutSync) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(false));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_EQ(nil, signout_coordinator_.title);
}

// Tests that a signed-in user with the forced sign-in policy enabled will have
// an action sheet with a title and a message.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithForcedSignin) {
  // Enable forced sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kForced));

  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(false));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_NE(nil, signout_coordinator_.title);
  ASSERT_NE(nil, signout_coordinator_.message);
}

// Tests that a signed-in managed user with Sync enabled will have an action
// sheet with a sign-out title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInManagedUserWithSync) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(true));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_NE(nil, signout_coordinator_.title);
}

// Tests that a signed-in managed user with Sync disabled will have an action
// sheet with no title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInManagedUserWithoutSync) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(testing::Return(false));

  CreateCoordinator();
  [signout_coordinator_ start];

  ASSERT_EQ(nil, signout_coordinator_.title);
}

class SignoutActionSheetCoordinatorTestWithReplaceSyncWithSigninEnabled
    : public SignoutActionSheetCoordinatorTest {
 public:
  SignoutActionSheetCoordinatorTestWithReplaceSyncWithSigninEnabled()
      : feature_list_(syncer::kReplaceSyncPromosWithSignInPromos) {}

  void SetUp() override {
    SignoutActionSheetCoordinatorTest::SetUp();
    snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
  }

  // Sign-out coordinator.
  SignoutActionSheetCoordinator* CreateCoordinator() {
    SignoutActionSheetCoordinatorTest::CreateCoordinator();
    signout_coordinator_.completion = ^(BOOL success) {
      completion_callback_.Run(success);
    };
    return signout_coordinator_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  id snackbar_handler_;
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>>
      completion_callback_;
};

TEST_F(SignoutActionSheetCoordinatorTestWithReplaceSyncWithSigninEnabled,
       ShouldNotShowActionSheetIfNoUnsyncedData) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateCoordinator();
  // Mock returning no unsynced datatype.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](base::OnceCallback<void(syncer::ModelTypeSet)> callback) {
            std::move(callback).Run(syncer::ModelTypeSet());
          });
  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  histogram_tester.ExpectTotalCount("Sync.UnsyncedDataOnSignout", 0u);
  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

TEST_F(SignoutActionSheetCoordinatorTestWithReplaceSyncWithSigninEnabled,
       ShouldShowActionSheetIfUnsyncedData) {
  authentication_service()->SignIn(
      identity_, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateCoordinator();
  // Mock returning no unsynced datatype.
  ON_CALL(*sync_service_mock_, GetTypesWithUnsyncedData)
      .WillByDefault(
          [](base::OnceCallback<void(syncer::ModelTypeSet)> callback) {
            std::move(callback).Run(
                syncer::ModelTypeSet({syncer::BOOKMARKS, syncer::PREFERENCES}));
          });
  EXPECT_CALL(completion_callback_, Run);

  base::HistogramTester histogram_tester;

  [signout_coordinator_ start];

  histogram_tester.ExpectTotalCount("Sync.UnsyncedDataOnSignout", 1u);
  histogram_tester.ExpectBucketCount(
      "Sync.UnsyncedDataOnSignout",
      syncer::ModelTypeForHistograms(syncer::BOOKMARKS), 1u);
  // Only a few "interesting" data types are recorded. PREFERENCES is not.
  histogram_tester.ExpectBucketCount(
      "Sync.UnsyncedDataOnSignout",
      syncer::ModelTypeForHistograms(syncer::PREFERENCES), 0u);

  histogram_tester.ExpectTotalCount("Sync.SignoutWithUnsyncedData", 0u);
}

// TODO(crbug.com/1496731): Add test for recording signout outcome upon warning
// dialog for unsynced data (i.e. for Sync.SignoutWithUnsyncedData).
