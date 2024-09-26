// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <utility>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_device_management_service.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "google_apis/gaia/gaia_constants.h"
#import "google_apis/gaia/gaia_urls.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/policy/device_management_service_configuration_ios.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "net/http/http_status_code.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::AnyNumber;
using testing::Mock;
using testing::SaveArg;

namespace policy {

constexpr char kManagedTestUser[] = "testuser@test.com";

constexpr char kUnmanagedTestUser[] = "testuser@gmail.com";

constexpr char kHostedDomainResponse[] = R"(
    {
      "hd": "test.com"
    })";

constexpr char kDmToken[] = "dm_token";

// Builds and returns a UserCloudPolicyManager for testing.
std::unique_ptr<UserCloudPolicyManager> BuildCloudPolicyManager() {
  auto store = std::make_unique<MockUserCloudPolicyStore>();
  EXPECT_CALL(*store, Load()).Times(AnyNumber());

  return std::make_unique<UserCloudPolicyManager>(
      std::move(store), base::FilePath(),
      /*cloud_external_data_manager=*/nullptr,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network::TestNetworkConnectionTracker::CreateGetter());
}

class UserPolicySigninServiceTest : public PlatformTest {
 public:
  UserPolicySigninServiceTest()
      : test_account_id_(AccountId::FromUserEmailGaiaId(
            kManagedTestUser,
            signin::GetTestGaiaIdForEmail(kManagedTestUser))),
        register_completed_(false) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

  // Called when the user policy registration is completed.
  void OnRegisterCompleted(const std::string& dm_token,
                           const std::string& client_id) {
    register_completed_ = true;
    dm_token_ = dm_token;
    client_id_ = client_id;
  }

  // Registers the `kManagedTestUser` for user policy.
  void RegisterPolicyClientWithCallback(UserPolicySigninService* service) {
    UserPolicySigninServiceBase::PolicyRegistrationCallback callback =
        base::BindOnce(&UserPolicySigninServiceTest::OnRegisterCompleted,
                       base::Unretained(this));
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable(kManagedTestUser);
    service->RegisterForPolicyWithAccountId(
        kManagedTestUser, account_info.account_id, std::move(callback));
    ASSERT_TRUE(IsRequestActive());
  }

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures({policy::kUserPolicy}, {});

    device_management_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
        &device_management_service_);

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    TestingApplicationContext::GetGlobal()->GetBrowserPolicyConnector()->Init(
        local_state_.get(),
        TestingApplicationContext::GetGlobal()->GetSharedURLLoaderFactory());

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterBrowserStatePrefs(prefs->registry());

    // Set the User Policy notification as seen by default.
    prefs->SetBoolean(policy::policy_prefs::kUserPolicyNotificationWasShown,
                      true);

    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(
            std::move(prefs)));
    builder.SetUserCloudPolicyManager(BuildCloudPolicyManager());
    browser_state_ = builder.Build();
    browser_state_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    manager_ = browser_state_->GetUserCloudPolicyManager();
    DCHECK(manager_);
    manager_->Init(&schema_registry_);
    mock_store_ =
        static_cast<MockUserCloudPolicyStore*>(manager_->core()->store());
    DCHECK(mock_store_);

    // Verify that the UserCloudPolicyManager core services aren't initialized
    // before creating the user policy service.
    ASSERT_FALSE(manager_->core()->service());
  }

  void TearDown() override {
    if (user_policy_signin_service_) {
      user_policy_signin_service_->Shutdown();
    }
    user_policy_signin_service_.reset();
    manager_->Shutdown();

    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
        nullptr);

    browser_state_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    TestingApplicationContext::GetGlobal()
        ->GetBrowserPolicyConnector()
        ->Shutdown();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Returns true if there is at least one request that is currently pending in
  // the URL loader.
  bool IsRequestActive() {
    if (identity_test_env()->IsAccessTokenRequestPending())
      return true;
    return test_url_loader_factory_.NumPending() > 0;
  }

  // Makes the oauth token fetch that is pending a success.
  void MakeOAuthTokenFetchSucceed() {
    ASSERT_TRUE(IsRequestActive());
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", base::Time::Now());
  }

  // Makes the oauth token fetch that is pending a failure.
  void MakeOAuthTokenFetchFail() {
    ASSERT_TRUE(identity_test_env()->IsAccessTokenRequestPending());
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError::FromServiceError("fail"));
  }

  // Makes the user info request respond with the hosted domain info. Set
  // `is_hosted_domain` to true to fill hosted domain info in the response. The
  // response will be empty is set to false.
  void ReportHostedDomainStatus(bool is_hosted_domain) {
    ASSERT_TRUE(IsRequestActive());
    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
        is_hosted_domain ? kHostedDomainResponse : "{}"));
  }

  // Simulates the flow that registrates an account for user policy.
  void RegisterForPolicyAndSignin() {
    EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(0);
    RegisterPolicyClientWithCallback(user_policy_signin_service_.get());

    // Sign in to Chrome.
    identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                           signin::ConsentLevel::kSync);

    DoPendingRegistration(/*with_dm_token=*/true,
                          /*with_oauth_token_success=*/true);

    EXPECT_TRUE(register_completed_);
    EXPECT_EQ(dm_token_, kDmToken);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  // Initialize the UserPolicySigninService by creating the instance of the
  // service that is hold by `user_policy_signin_service_`.
  void InitUserPolicySigninService() {
    user_policy_signin_service_ = std::make_unique<UserPolicySigninService>(
        browser_state_->GetPrefs(), local_state_.get(),
        &device_management_service_,
        browser_state_->GetUserCloudPolicyManager(),
        identity_test_env_.identity_manager(),
        browser_state_->GetSharedURLLoaderFactory());
  }

  // Does and complete the registration job that was queued and that is
  // currently pending.
  void DoPendingRegistration(bool with_dm_token,
                             bool with_oauth_token_success) {
    if (with_oauth_token_success) {
      MakeOAuthTokenFetchSucceed();
    } else {
      MakeOAuthTokenFetchFail();
      ASSERT_FALSE(IsRequestActive());
      return;
    }

    // When the user is from a hosted domain, this should kick off client
    // registration.
    DeviceManagementService::JobConfiguration::JobType job_type =
        DeviceManagementService::JobConfiguration::TYPE_INVALID;
    DeviceManagementService::JobForTesting job;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                        SaveArg<0>(&job)));

    // Mimic the user being a hosted domain - this should cause a Register()
    // call.
    ReportHostedDomainStatus(true);

    // Should have no more outstanding requests.
    ASSERT_FALSE(IsRequestActive());
    Mock::VerifyAndClearExpectations(this);
    ASSERT_TRUE(job.IsActive());
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
              job_type);

    enterprise_management::DeviceManagementResponse registration_response;
    if (with_dm_token) {
      registration_response.mutable_register_response()
          ->set_device_management_token(kDmToken);
      registration_response.mutable_register_response()->set_enrollment_type(
          enterprise_management::DeviceRegisterResponse::ENTERPRISE);
    }
    device_management_service_.SendJobOKNow(&job, registration_response);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  raw_ptr<MockUserCloudPolicyStore> mock_store_ = nullptr;  // Not owned.
  SchemaRegistry schema_registry_;
  raw_ptr<UserCloudPolicyManager> manager_ = nullptr;  // Not owned.

  // Used in conjunction with OnRegisterCompleted() to test client registration
  // callbacks.
  std::string dm_token_;
  std::string client_id_;

  // AccountId for the test user.
  AccountId test_account_id_;

  // True if OnRegisterCompleted() was called.
  bool register_completed_;

  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService device_management_service_{
      &job_creation_handler_};

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<UserPolicySigninService> user_policy_signin_service_;
};

// Tests that the user policy manager isn't initialized when initializing the
// user policy service with a user that isn't syncing.
TEST_F(UserPolicySigninServiceTest,
       DontRegisterDuringInitializationBecauseUserSignedOut) {
  // Verify that the user isn't syncing before starting the user policy
  // service.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));

  // Initialize UserPolicySigninService without a syncing account which should
  // result in shutting down the manager.
  EXPECT_CALL(*mock_store_, Clear());
  InitUserPolicySigninService();
  Mock::VerifyAndClearExpectations(mock_store_);

  // Expect that the UserCloudPolicyManager isn't initialized because the user
  // wasn't syncing, hence not eligible for user policy.
  EXPECT_FALSE(manager_->core()->service());
}

// Tests that the user policy manager isn't initialized when initializing the
// user policy service with a user that is syncing with an unmanaged account
// that is not eligible for user policy.
TEST_F(UserPolicySigninServiceTest,
       DontRegisterDuringInitializationBecauseUnmanagedAccount) {
  // Set the user as signed in and syncing with an unmanaged account.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kUnmanagedTestUser);
  identity_test_env()->SetPrimaryAccount(kUnmanagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Initialize the UserPolicySigninService with a syncing account that is
  // unmanaged which should result in shutting down the manager.
  InitUserPolicySigninService();

  // Expect that the UserCloudPolicyManager isn't initialized because the user
  // was using an unmanaged account, hence not eligible for user policy.
  EXPECT_FALSE(manager_->core()->service());
}

// Tests that the user policy manager isn't initialized when initializing the
// user policy service with a user that is signed in but not syncing.
TEST_F(UserPolicySigninServiceTest,
       DontRegisterDuringInitializationBecauseSignedInButNotSynced) {
  // Set the user as signed in with a managed account.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSignin);

  // Initialize UserPolicySigninService with a signed in account that is
  // not syncing which should result in shutting down the manager.
  EXPECT_CALL(*mock_store_, Clear());
  InitUserPolicySigninService();
  Mock::VerifyAndClearExpectations(mock_store_);

  // Expect that the UserCloudPolicyManager isn't initialized because the user
  // was signed in with a managed account but not syncing, hence not eligible
  // for user policy.
  EXPECT_FALSE(manager_->core()->service());
}

// Tests that the user policy manager isn't initialized when the user policy
// feature is disabled despite the account being eligible for user policy.
TEST_F(UserPolicySigninServiceTest,
       DontRegisterDuringInitializationBecauseFeatureDisabled) {
  // Disable the user policy features by clearing the scoped feature list.
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();

  // Set the user as syncing with a managed account.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Initialize UserPolicySigninService when the feature is disabled which
  // will result in shutting down the manager and clearing the store.
  EXPECT_CALL(*mock_store_, Clear());
  InitUserPolicySigninService();
  Mock::VerifyAndClearExpectations(mock_store_);

  // Expect that the UserCloudPolicyManager isn't initialized because the user
  // was signed in with a managed account but the user policy feature was
  // disabled.
  EXPECT_FALSE(manager_->core()->service());
}

// Tests that the registration for user policy and the initialization of the
// user policy manager can be done during the initialization of the user policy
// service when the user is already syncing and eligible for user policy.
TEST_F(UserPolicySigninServiceTest,
       RegisterAndInitializeManagerDuringInitialization) {
  // Set the user as signed in and syncing.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Mark the store as loaded to allow registration during the initialization of
  // the user policy service.
  mock_store_->NotifyStoreLoaded();

  // Initialize the UserPolicySigninService while the user has sync enabled and
  // is eligible for user policy. This will kick off the asynchronous
  // registration process.
  InitUserPolicySigninService();

  // Run the delayed task to start the registration by fast forwarding the task
  // runner clock.
  task_environment_.FastForwardBy(
      GetTryRegistrationDelayFromPrefs(browser_state_->GetPrefs()));

  // Do the pending registration that was queued in the initialization of the
  // service.
  DoPendingRegistration(/*with_dm_token=*/true,
                        /*with_oauth_token_success=*/true);
  // Verify that the client is registered after the initialization.
  ASSERT_TRUE(manager_->core()->client()->is_registered());

  // Expect the UserCloudPolicyManager to be initialized when creating the
  // service because the user is syncing and eligible for user policy.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Expect sign-out to clear the policy from the store and shutdown the
  // UserCloudPolicyManager.
  EXPECT_CALL(*mock_store_, Clear());
  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(manager_->core()->service());
}

// Tests that registration is still possible after the manager was shutdown
// because of sign-out.
TEST_F(UserPolicySigninServiceTest, CanRegisterAfterSignOut) {
  // Explicitly forcing this call is necessary for the clearing of the primary
  // account to result in the account being fully removed in this testing
  // context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();

  // Set the user as signed in and syncing.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Mark the store as loaded to allow registration during the initialization of
  // the user policy service.
  mock_store_->NotifyStoreLoaded();

  // Initialize the UserPolicySigninService while the user has sync enabled and
  // is eligible for user policy. This will kick off the asynchronous
  // registration process.
  InitUserPolicySigninService();

  // Register.
  task_environment_.FastForwardBy(
      GetTryRegistrationDelayFromPrefs(browser_state_->GetPrefs()));
  DoPendingRegistration(/*with_dm_token=*/true,
                        /*with_oauth_token_success=*/true);
  ASSERT_TRUE(manager_->core()->client()->is_registered());

  // Expect the UserCloudPolicyManager to be initialized when creating the
  // service because the user is syncing and eligible for user policy.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Sign out. This should shutdown the manager and clear the policy data store.
  EXPECT_CALL(*mock_store_, Clear());
  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(manager_->core()->service());

  // Verify the registration can still be done.
  RegisterForPolicyAndSignin();
  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, kDmToken);
}

// Tests that registration errors can be handled.
TEST_F(UserPolicySigninServiceTest, CanHandleRegisterError) {
  // Explicitly forcing this call is necessary for the clearing of the primary
  // account to result in the account being fully removed in this testing
  // context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();

  // Set the user as signed in and syncing.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Mark the store as loaded to allow registration during the initialization of
  // the user policy service.
  mock_store_->NotifyStoreLoaded();

  // Initialize the UserPolicySigninService while the user has sync enabled and
  // is eligible for user policy. This will kick off the asynchronous
  // registration process.
  InitUserPolicySigninService();

  // Register with failure.
  task_environment_.FastForwardBy(
      GetTryRegistrationDelayFromPrefs(browser_state_->GetPrefs()));
  DoPendingRegistration(/*with_dm_token=*/false,
                        /*with_oauth_token_success=*/true);

  // Verify that the client doesn't declare itself as registered.
  ASSERT_FALSE(manager_->core()->client()->is_registered());

  // The manager should still be initialized despite the failed registration.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
}

// Tests that oauth token errors can be handled.
TEST_F(UserPolicySigninServiceTest, CanHandleOauthTokenError) {
  // Explicitly forcing this call is necessary for the clearing of the primary
  // account to result in the account being fully removed in this testing
  // context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();

  // Set the user as signed in and syncing.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Mark the store as loaded to allow registration during the initialization of
  // the user policy service.
  mock_store_->NotifyStoreLoaded();

  // Initialize the UserPolicySigninService while the user has sync enabled and
  // is eligible for user policy. This will kick off the asynchronous
  // registration process.
  InitUserPolicySigninService();

  // Register with failure.
  task_environment_.FastForwardBy(
      GetTryRegistrationDelayFromPrefs(browser_state_->GetPrefs()));
  DoPendingRegistration(/*with_dm_token=*/true,
                        /*with_oauth_token_success=*/false);

  // Verify that the client doesn't declare itself as registered.
  ASSERT_FALSE(manager_->core()->client()->is_registered());

  // The manager should still be initialized despite the failed registration.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
}

// Tests that the user policy manager isn't initialized when initializing the
// user policy service if the user hasn't seen the User Policy notification,
// even if the user is syncing with a managed account.
TEST_F(UserPolicySigninServiceTest,
       DontRegisterDuringInitializationBecauseUserHasntSeenNotification) {
  // Set the managed account as signed in and syncing.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(kManagedTestUser);
  identity_test_env()->SetPrimaryAccount(kManagedTestUser,
                                         signin::ConsentLevel::kSync);

  // Mark the store as loaded to allow registration during the initialization of
  // the user policy service.
  mock_store_->NotifyStoreLoaded();

  // Set the User Policy notification has not seen.
  browser_state_->GetPrefs()->SetBoolean(
      policy::policy_prefs::kUserPolicyNotificationWasShown, false);

  // Initialize the UserPolicySigninService while the user has sync enabled,
  // but hasn't seen the notification.
  InitUserPolicySigninService();

  // Expect that the UserCloudPolicyManager isn't initialized because the user
  // hasn't seen the notification yet.
  EXPECT_FALSE(manager_->core()->service());
}

}  // namespace policy
