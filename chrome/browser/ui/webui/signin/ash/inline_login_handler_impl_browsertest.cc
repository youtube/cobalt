// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr char kEmailKey[] = "email";
constexpr char kPasswordKey[] = "password";
constexpr char kGaiaIdKey[] = "gaiaId";
constexpr char kIsAvailableInArcKey[] = "isAvailableInArc";
constexpr char kSecondaryAccount1Email[] = "secondary1@gmail.com";
constexpr char kSecondaryAccount2Email[] = "secondary2@gmail.com";
constexpr char kSecondaryAccount3Email[] = "secondary3@gmail.com";
constexpr char kSecondaryAccountOAuthCode[] = "fake_oauth_code";
constexpr char kSecondaryAccountRefreshToken[] = "fake_refresh_token";
constexpr char kCompleteLoginMessage[] = "completeLogin";
constexpr char kMakeAvailableInArcMessage[] = "makeAvailableInArc";
constexpr char kGetAccountsNotAvailableInArcMessage[] =
    "getAccountsNotAvailableInArc";
constexpr char kHandleFunctionName[] = "handleFunctionName";
constexpr char kConsentLoggedCallback[] = "consent-logged-callback";
constexpr char kToSVersion[] = "12345678";

struct DeviceAccountInfo {
  std::string id;
  std::string email;

  user_manager::UserType user_type;
  account_manager::AccountType account_type;
  std::string token;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const DeviceAccountInfo& device_account_info);
};

std::ostream& operator<<(std::ostream& stream,
                         const DeviceAccountInfo& device_account_info) {
  return stream << "{email: " << device_account_info.email
                << ", user_type: " << device_account_info.user_type << "}";
}

DeviceAccountInfo GetGaiaDeviceAccountInfo() {
  return {signin::GetTestGaiaIdForEmail("primary@gmail.com") /*id*/,
          "primary@gmail.com" /*email*/,
          user_manager::USER_TYPE_REGULAR /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

DeviceAccountInfo GetChildDeviceAccountInfo() {
  return {supervised_user::kChildAccountSUID /*id*/,
          "child@gmail.com" /*email*/,
          user_manager::USER_TYPE_CHILD /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

base::Value GetCompleteLoginArgs(const std::string& email) {
  base::Value::Dict dict;
  dict.Set(kEmailKey, base::Value(email));
  dict.Set(kPasswordKey, base::Value("fake password"));
  dict.Set(kGaiaIdKey, base::Value(signin::GetTestGaiaIdForEmail(email)));
  dict.Set(kIsAvailableInArcKey, base::Value(true));
  return base::Value(std::move(dict));
}

MATCHER_P(AccountEmailEq, expected_email, "") {
  return testing::ExplainMatchResult(
      testing::Field(&account_manager::Account::raw_email,
                     testing::StrEq(expected_email)),
      arg, result_listener);
}

class TestInlineLoginHandler : public InlineLoginHandlerImpl {
 public:
  TestInlineLoginHandler(const base::RepeatingClosure& close_dialog_closure,
                         content::WebUI* web_ui)
      : InlineLoginHandlerImpl(close_dialog_closure) {
    set_web_ui(web_ui);
  }

  TestInlineLoginHandler(const TestInlineLoginHandler&) = delete;
  TestInlineLoginHandler& operator=(const TestInlineLoginHandler&) = delete;
};

class MockAccountAppsAvailabilityObserver
    : public AccountAppsAvailability::Observer {
 public:
  MockAccountAppsAvailabilityObserver() = default;
  ~MockAccountAppsAvailabilityObserver() override = default;

  MOCK_METHOD(void,
              OnAccountAvailableInArc,
              (const account_manager::Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountUnavailableInArc,
              (const account_manager::Account&),
              (override));
};

}  // namespace

class InlineLoginHandlerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<DeviceAccountInfo> {
 public:
  InlineLoginHandlerTest()
      : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }
  ~InlineLoginHandlerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Configure embedded test server.
    const GURL& base_url = embedded_test_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    fake_gaia_.Initialize();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &InlineLoginHandlerTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    embedded_test_server_.StartAcceptingConnections();

    // Setup user.
    auto user_manager = std::make_unique<FakeChromeUserManager>();
    const user_manager::User* user;
    if (GetDeviceAccountInfo().user_type ==
        user_manager::UserType::USER_TYPE_CHILD) {
      user = user_manager->AddChildUser(AccountId::FromUserEmailGaiaId(
          GetDeviceAccountInfo().email, GetDeviceAccountInfo().id));
      profile()->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                       GetDeviceAccountInfo().id);
      // This is required for Child users, otherwise an account cannot be added.
      edu_handler_ =
          std::make_unique<EduCoexistenceLoginHandler>(base::DoNothing());
      edu_handler_->set_web_ui_for_test(web_ui());
      edu_handler_->RegisterMessages();
    } else {
      user = user_manager->AddUser(AccountId::FromUserEmailGaiaId(
          GetDeviceAccountInfo().email, GetDeviceAccountInfo().id));
    }
    primary_account_id_ = user->GetAccountId();
    user_manager->LoginUser(primary_account_id_);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile());

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_profile_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(GetDeviceAccountInfo().email,
                                      signin::ConsentLevel::kSignin);

    // Setup web ui with cookies.
    web_ui_.set_web_contents(web_contents());
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    auto url = GaiaUrls::GetInstance()->gaia_url();
    auto cookie_obj = net::CanonicalCookie::Create(
        url, std::string("oauth_code=") + kSecondaryAccountOAuthCode,
        base::Time::Now(), absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */);
    content::StoragePartition* partition =
        signin::GetSigninPartition(web_contents()->GetBrowserContext());
    base::test::TestFuture<net::CookieAccessResult> future;
    partition->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
        *cookie_obj, url, options, future.GetCallback());
    EXPECT_TRUE(future.Wait());

    // Setup fake Gaia.
    FakeGaia::Configuration params;
    params.email = kSecondaryAccount1Email;
    params.refresh_token = kSecondaryAccountRefreshToken;
    params.auth_code = kSecondaryAccountOAuthCode;
    fake_gaia_.UpdateConfiguration(params);

    // Setup handlers.
    handler_ =
        std::make_unique<TestInlineLoginHandler>(base::DoNothing(), &web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    edu_handler_.reset();
    identity_test_env_profile_adaptor_.reset();
    base::RunLoop().RunUntilIdle();
    GetFakeUserManager()->RemoveUserFromList(primary_account_id_);
    user_manager_enabler_.reset();
  }

  void CompleteConsentLogForChildUser(const std::string& secondary_email) {
    base::Value::List call_args;
    call_args.Append(secondary_email);
    call_args.Append(kToSVersion);

    base::Value::List list_args;
    list_args.Append(kConsentLoggedCallback);
    list_args.Append(std::move(call_args));

    web_ui()->HandleReceivedMessage("consentLogged", list_args);
  }

  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  DeviceAccountInfo GetDeviceAccountInfo() const { return GetParam(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() { return browser()->profile(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<InlineLoginHandler> handler_;
  std::unique_ptr<EduCoexistenceLoginHandler> edu_handler_;
  content::TestWebUI web_ui_;
  net::EmbeddedTestServer embedded_test_server_;
  FakeGaia fake_gaia_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  AccountId primary_account_id_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest, NewAccountAdditionSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(::GetAccountManagerFacade(profile()->GetPath().value()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(kSecondaryAccount1Email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  if (GetDeviceAccountInfo().user_type ==
      user_manager::UserType::USER_TYPE_CHILD) {
    // Consent logging is required for secondary accounts.
    CompleteConsentLogForChildUser(kSecondaryAccount1Email);
  }

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(kSecondaryAccount1Email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest, PrimaryReauthenticationSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(::GetAccountManagerFacade(profile()->GetPath().value()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(GetDeviceAccountInfo().email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(GetDeviceAccountInfo().email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

INSTANTIATE_TEST_SUITE_P(InlineLoginHandlerTestSuite,
                         InlineLoginHandlerTest,
                         ::testing::Values(GetGaiaDeviceAccountInfo(),
                                           GetChildDeviceAccountInfo()));

class InlineLoginHandlerTestWithArcRestrictions
    : public InlineLoginHandlerTest {
 public:
  InlineLoginHandlerTestWithArcRestrictions() {
    auto lacros = ash::standalone_browser::GetFeatureRefs();
    lacros.push_back(
        ash::standalone_browser::features::kLacrosForSupervisedUsers);
    feature_list_.InitWithFeatures(/*enabled=*/lacros, /*disabled=*/{});
  }

  ~InlineLoginHandlerTestWithArcRestrictions() override = default;

  void SetUpOnMainThread() override {
    if (browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work
      // even when Lacros is the only browser.
      // TODO(crbug.com/1450158): Remove uses of browser() from such tests.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      SelectFirstBrowser();
    }
    InlineLoginHandlerTest::SetUpOnMainThread();
    // In-session account addition happens when `AccountAppsAvailability` is
    // already initialized.
    EXPECT_TRUE(AccountAppsAvailabilityFactory::GetForProfile(profile())
                    ->IsInitialized());
  }

  void AddAccount(const std::string& email, bool is_available_in_arc) {
    account_manager::MockAccountManagerFacadeObserver observer;
    base::ScopedObservation<account_manager::AccountManagerFacade,
                            account_manager::AccountManagerFacade::Observer>
        observation{&observer};
    observation.Observe(
        ::GetAccountManagerFacade(profile()->GetPath().value()));
    auto* account_apps_availability =
        AccountAppsAvailabilityFactory::GetForProfile(profile());

    base::test::TestFuture<void> future;
    EXPECT_CALL(observer, OnAccountUpserted(AccountEmailEq(email)))
        .WillOnce(testing::DoAll(
            base::test::RunOnceClosure(future.GetCallback()),
            [account_apps_availability,
             is_available_in_arc](const account_manager::Account& account) {
              account_apps_availability->SetIsAccountAvailableInArc(
                  account, is_available_in_arc);
            }));
    identity_test_env()->MakeAccountAvailable(email);
    EXPECT_TRUE(future.Wait());
  }

  bool ValuesListContainAccount(const base::Value::List& values,
                                const std::string& email) {
    return ValuesListGetAccount(values, email).has_value();
  }

  absl::optional<base::Value> ValuesListGetAccount(
      const base::Value::List& values,
      const std::string& email) {
    for (const base::Value& value : values) {
      const std::string* email_val = value.GetDict().FindString("email");
      EXPECT_TRUE(email_val != nullptr);
      if (*email_val == email)
        return value.Clone();
    }
    return absl::nullopt;
  }

  const base::Value::List& CallGetAccountsNotAvailableInArc() {
    // Call "getAccountsNotAvailableInArc".
    base::Value::List args;
    args.Append(kHandleFunctionName);
    web_ui()->HandleReceivedMessage(kGetAccountsNotAvailableInArcMessage, args);
    base::RunLoop().RunUntilIdle();

    const content::TestWebUI::CallData& call_data =
        *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());

    // Get results from JS callback.
    return call_data.arg3()->GetList();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTestWithArcRestrictions,
                       NewAccountAdditionSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation{&observer};
  account_manager_facade_observation.Observe(
      ::GetAccountManagerFacade(profile()->GetPath().value()));

  MockAccountAppsAvailabilityObserver apps_availability_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      apps_availability_observation{&apps_availability_observer};
  apps_availability_observation.Observe(
      AccountAppsAvailabilityFactory::GetForProfile(profile()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(kSecondaryAccount1Email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  if (GetDeviceAccountInfo().user_type ==
      user_manager::UserType::USER_TYPE_CHILD) {
    // Consent logging is required for secondary accounts.
    CompleteConsentLogForChildUser(kSecondaryAccount1Email);
  }

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(kSecondaryAccount1Email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());

  // Make sure that account was added to ARC.
  base::test::TestFuture<void> future2;
  EXPECT_CALL(apps_availability_observer,
              OnAccountAvailableInArc(AccountEmailEq(kSecondaryAccount1Email)))
      .WillOnce(base::test::RunOnceClosure(future2.GetCallback()));
  EXPECT_TRUE(future2.Wait());
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTestWithArcRestrictions,
                       PrimaryReauthenticationSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation{&observer};
  account_manager_facade_observation.Observe(
      ::GetAccountManagerFacade(profile()->GetPath().value()));

  MockAccountAppsAvailabilityObserver apps_availability_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      apps_availability_observation{&apps_availability_observer};
  apps_availability_observation.Observe(
      AccountAppsAvailabilityFactory::GetForProfile(profile()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(GetDeviceAccountInfo().email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(GetDeviceAccountInfo().email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());

  // Make sure that ARC availability didn't change for account.
  EXPECT_CALL(apps_availability_observer, OnAccountAvailableInArc).Times(0);
  EXPECT_CALL(apps_availability_observer, OnAccountUnavailableInArc).Times(0);
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTestWithArcRestrictions,
                       GetAccountsNotAvailableInArc) {
  AddAccount(kSecondaryAccount1Email, /*is_available_in_arc=*/true);
  AddAccount(kSecondaryAccount2Email, /*is_available_in_arc=*/false);
  AddAccount(kSecondaryAccount3Email, /*is_available_in_arc=*/false);

  // Call "getAccountsNotAvailableInArc".
  const base::Value::List& result = CallGetAccountsNotAvailableInArc();
  // Two accounts are not available in ARC.
  EXPECT_EQ(2u, result.size());
  EXPECT_FALSE(ValuesListContainAccount(result, kSecondaryAccount1Email));
  EXPECT_TRUE(ValuesListContainAccount(result, kSecondaryAccount2Email));
  EXPECT_TRUE(ValuesListContainAccount(result, kSecondaryAccount3Email));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTestWithArcRestrictions,
                       MakeAvailableInArc) {
  AddAccount(kSecondaryAccount1Email, /*is_available_in_arc=*/true);
  AddAccount(kSecondaryAccount2Email, /*is_available_in_arc=*/false);

  // Call "getAccountsNotAvailableInArc".
  const base::Value::List& result = CallGetAccountsNotAvailableInArc();
  // One account is not available in ARC.
  EXPECT_EQ(1u, result.size());
  EXPECT_FALSE(ValuesListContainAccount(result, kSecondaryAccount1Email));
  EXPECT_TRUE(ValuesListContainAccount(result, kSecondaryAccount2Email));

  // Call "makeAvailableInArc".
  base::Value::List args_1;
  args_1.Append(ValuesListGetAccount(result, kSecondaryAccount2Email).value());
  web_ui()->HandleReceivedMessage(kMakeAvailableInArcMessage, args_1);

  // Call "getAccountsNotAvailableInArc".
  const base::Value::List& result_1 = CallGetAccountsNotAvailableInArc();
  // Zero accounts are not available in ARC.
  EXPECT_EQ(0u, result_1.size());
}

INSTANTIATE_TEST_SUITE_P(InlineLoginHandlerTestWithArcRestrictionsSuite,
                         InlineLoginHandlerTestWithArcRestrictions,
                         ::testing::Values(GetGaiaDeviceAccountInfo(),
                                           GetChildDeviceAccountInfo()));

}  // namespace ash
