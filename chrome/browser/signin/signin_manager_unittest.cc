// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/signin_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;

namespace signin {
namespace {

const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";

class FakeIdentityManagerObserver : public IdentityManager::Observer {
 public:
  explicit FakeIdentityManagerObserver(IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_observation_.Observe(identity_manager_);
  }

  ~FakeIdentityManagerObserver() override = default;

  void OnPrimaryAccountChanged(
      const PrimaryAccountChangeEvent& event) override {
    auto current_state = event.GetCurrentState();
    EXPECT_EQ(
        current_state.primary_account,
        identity_manager_->GetPrimaryAccountInfo(current_state.consent_level));
    events_.push_back(event);
  }

  const std::vector<PrimaryAccountChangeEvent>& events() const {
    return events_;
  }

  void Reset() { events_.clear(); }

 private:
  raw_ptr<IdentityManager> identity_manager_;
  std::vector<PrimaryAccountChangeEvent> events_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace

class SigninManagerTest : public testing::Test,
                          public ::testing::WithParamInterface<bool> {
 public:
  SigninManagerTest()
      : client_(&prefs_),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           /*pref_service=*/&prefs_,
                           signin::AccountConsistencyMethod::kDice,
                           &client_),
        observer_(identity_test_env_.identity_manager()) {
    RecreateSigninManager();
  }

  SigninManagerTest(const SigninManagerTest&) = delete;
  SigninManagerTest& operator=(const SigninManagerTest&) = delete;

  void RecreateSigninManager() {
    signin_manager_ =
        std::make_unique<SigninManager>(&prefs_, identity_manager(), &client_);
  }

  void InitializeSignoutDecision() {
    if (!is_signout_allowed()) {
      client_.set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
    }
  }

  bool is_signout_allowed() const { return GetParam(); }

  AccountInfo GetAccountInfo(const std::string& email) {
    AccountInfo account_info;
    account_info.gaia = GetTestGaiaIdForEmail(email);
    account_info.account_id =
        identity_manager()->PickAccountIdForAccount(account_info.gaia, email);
    account_info.email = email;
    return account_info;
  }

  void ExpectUnconsentedPrimaryAccountSetEvent(
      const CoreAccountInfo& expected_primary_account) {
    EXPECT_EQ(1U, observer().events().size());
    auto event = observer().events()[0];
    EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
              event.GetEventTypeFor(ConsentLevel::kSignin));
    EXPECT_TRUE(event.GetPreviousState().primary_account.IsEmpty());
    EXPECT_EQ(expected_primary_account,
              event.GetCurrentState().primary_account);
    observer().Reset();
  }

  void ExpectUnconsentedPrimaryAccountChangedEvent(
      const CoreAccountInfo& expected_previous_account,
      const CoreAccountInfo& expected_current_account) {
    EXPECT_EQ(1U, observer().events().size());
    auto event = observer().events()[0];
    EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
              event.GetEventTypeFor(ConsentLevel::kSignin));
    EXPECT_EQ(expected_previous_account,
              event.GetPreviousState().primary_account);
    EXPECT_EQ(expected_current_account,
              event.GetCurrentState().primary_account);
    observer().Reset();
  }

  void ExpectUnconsentedPrimaryAccountClearedEvent(
      const CoreAccountInfo& expected_cleared_account) {
    EXPECT_EQ(1U, observer().events().size());
    auto event = observer().events()[0];
    EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
              event.GetEventTypeFor(ConsentLevel::kSignin));
    EXPECT_EQ(expected_cleared_account,
              event.GetPreviousState().primary_account);
    EXPECT_TRUE(event.GetCurrentState().primary_account.IsEmpty());
    observer().Reset();
  }

  void ExpectSyncPrimaryAccountSetEvent(
      const CoreAccountInfo& expected_primary_account) {
    EXPECT_EQ(1U, observer().events().size());
    auto event = observer().events()[0];
    EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
              event.GetEventTypeFor(ConsentLevel::kSignin));
    EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
              event.GetEventTypeFor(ConsentLevel::kSync));
    EXPECT_TRUE(event.GetPreviousState().primary_account.IsEmpty());
    EXPECT_EQ(expected_primary_account,
              event.GetCurrentState().primary_account);
    observer().Reset();
  }

  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

  AccountInfo MakeAccountAvailableWithCookies(const std::string& email) {
    AccountInfo account = GetAccountInfo(kTestEmail);
    identity_test_env_.MakeAccountAvailableWithCookies(account.email,
                                                       account.gaia);
    EXPECT_FALSE(account.IsEmpty());
    EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
    EXPECT_EQ(account,
              identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
    EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
    return account;
  }

  AccountInfo MakeSyncAccountAvailableWithCookies(const std::string& email) {
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSync);
    identity_test_env_.SetCookieAccounts({{account.email, account.gaia}});
    EXPECT_EQ(account,
              identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
    EXPECT_EQ(account,
              identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync));
    EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
        signin::ConsentLevel::kSync));
    return account;
  }

  FakeIdentityManagerObserver& observer() { return observer_; }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  content::BrowserTaskEnvironment task_environment_;
  TestSigninClient client_;
  IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<SigninManager> signin_manager_;
  FakeIdentityManagerObserver observer_;
};

TEST_P(
    SigninManagerTest,
    UnconsentedPrimaryAccountUpdatedOnItsAccountRefreshTokenUpdateWithValidTokenWhenNoSyncConsent) {
  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  EXPECT_EQ(account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
}

TEST_P(
    SigninManagerTest,
    UnconsentedPrimaryAccountUpdatedOnItsAccountRefreshTokenUpdateWithInvalidTokenWhenNoSyncConsent) {
  // Prerequisite: add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // Invalid token.
  SetInvalidRefreshTokenForAccount(identity_manager(), account.account_id);

  if (is_signout_allowed()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros token service does not check for the validity of tokens.
  // Therefore, the primary account should not be removed.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
#else
    ExpectUnconsentedPrimaryAccountClearedEvent(account);
    EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
    // Update with a valid token.
    SetRefreshTokenForAccount(identity_manager(), account.account_id, "");
    ExpectUnconsentedPrimaryAccountSetEvent(account);
    EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin),
              account);
#endif
  } else {
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin),
            account);
  }
}

TEST_P(
    SigninManagerTest,
    UnconsentedPrimaryAccountRemovedOnItsAccountRefreshTokenRemovalWhenNoSyncConsent) {
  // Prerequisite: Add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // With no refresh token, there is no unconsented primary account any more.
  identity_test_env()->RemoveRefreshTokenForAccount(account.account_id);
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_P(SigninManagerTest, UnconsentedPrimaryAccountChangedBlockedByHandle) {
  // Prerequisite: Add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // Take a handle, then signal to clear the account.
  auto handle = signin_manager_->CreateAccountSelectionInProgressHandle();
  identity_test_env()->RemoveRefreshTokenForAccount(account.account_id);

  EXPECT_TRUE(observer().events().empty());
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // The account gets cleared once we destroy the handle.
  handle.reset();
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_P(SigninManagerTest, UnconsentedPrimaryAccountNotChangedOnSignout) {
  // Set a primary account at sync consent level.
  AccountInfo account = MakeSyncAccountAvailableWithCookies(kTestEmail);
  EXPECT_EQ(account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Verify the primary account changed event.
  ExpectSyncPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // Tests that sync primary account is cleared, but unconsented account is not.
  identity_test_env()->RevokeSyncConsent();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  EXPECT_EQ(1U, observer().events().size());
  auto event = observer().events()[0];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kNone,
            event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(account, event.GetPreviousState().primary_account);
  EXPECT_EQ(account, event.GetCurrentState().primary_account);
}

// Lacros does not use the cookies to compute the primary account.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(SigninManagerTest,
       UnconsentedPrimaryAccountTokenRevokedWithStaleCookies) {
  // Prerequisite: add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // Make the cookies stale and remove the account.
  // Removing the refresh token for the unconsented primary account is
  // sufficient to clear it.
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  identity_test_env()->RemoveRefreshTokenForAccount(account.account_id);
  ASSERT_FALSE(identity_manager()->GetAccountsInCookieJar().accounts_are_fresh);

  // Unconsented account was removed.
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_P(SigninManagerTest,
       UnconsentedPrimaryAccountTokenRevokedWithStaleCookiesMultipleAccounts) {
  // Add two accounts with cookies.
  AccountInfo main_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);
  identity_test_env()->SetCookieAccounts(
      {{main_account.email, main_account.gaia},
       {secondary_account.email, secondary_account.gaia}});

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(main_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  ExpectUnconsentedPrimaryAccountSetEvent(main_account);
  InitializeSignoutDecision();

  // Make the cookies stale and remove the main account.
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  identity_test_env()->RemoveRefreshTokenForAccount(main_account.account_id);
  ASSERT_FALSE(identity_manager()->GetAccountsInCookieJar().accounts_are_fresh);

  // Unconsented account was removed.
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(main_account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
}

TEST_P(SigninManagerTest,
       SameUnconsentedPrimaryAccountTokenWithRemovedCookies) {
  // Prerequisite: add an unconsented primary account, incl. proper cookies.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);
  InitializeSignoutDecision();

  // Set Gaia accounts in the cookie to empty.
  identity_test_env()->SetCookieAccounts({});
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(SigninManagerTest, UnconsentedPrimaryAccountDuringLoad) {
  // Pre-requisite: Add two accounts with cookies.
  AccountInfo main_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);
  identity_test_env()->SetCookieAccounts(
      {{main_account.email, main_account.gaia},
       {secondary_account.email, secondary_account.gaia}});
  ASSERT_EQ(main_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  ExpectUnconsentedPrimaryAccountSetEvent(main_account);
  InitializeSignoutDecision();

  // Set the token service in "loading" mode.
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  RecreateSigninManager();

  // Unconsented primary account is available while tokens are not loaded.
  EXPECT_EQ(main_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_TRUE(observer().events().empty());

  // Revoking an unrelated token doesn't change the unconsented primary account.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account.account_id);
  EXPECT_EQ(main_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_TRUE(observer().events().empty());

  // Revoking the token of the unconsented primary account while the tokens
  // are still loading does not change the unconsented primary account.
  identity_test_env()->RemoveRefreshTokenForAccount(main_account.account_id);
  EXPECT_EQ(main_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_TRUE(observer().events().empty());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Assert secondary profile.
  ASSERT_FALSE(client_.GetInitialPrimaryAccount().has_value());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  // Finish the token load should clear the primary account as the token of the
  // primary account was revoked.
  identity_test_env()->ReloadAccountsFromDisk();
  EXPECT_NE(is_signout_allowed(),
            identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  if (is_signout_allowed()) {
  ExpectUnconsentedPrimaryAccountClearedEvent(main_account);
  } else {
  EXPECT_EQ(0U, observer().events().size());
  }
}

TEST_P(SigninManagerTest,
       UnconsentedPrimaryAccountUpdatedOnSyncConsentRevoked) {
  AccountInfo first_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo second_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);
  identity_test_env()->SetCookieAccounts(
      {{first_account.email, first_account.gaia},
       {second_account.email, second_account.gaia}});
  ASSERT_EQ(first_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  ExpectUnconsentedPrimaryAccountSetEvent(first_account);

  // Set the sync primary account to the second account in cookies.
  // The unconsented primary account should be updated.
  identity_test_env()->SetPrimaryAccount(second_account.email,
                                         signin::ConsentLevel::kSync);
  InitializeSignoutDecision();

  EXPECT_EQ(second_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_EQ(1U, observer().events().size());
  auto event = observer().events()[0];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(first_account, event.GetPreviousState().primary_account);
  EXPECT_EQ(second_account, event.GetCurrentState().primary_account);
  observer().Reset();

  // Clear primary account but do not delete the account. The unconsented
  // primary account should be updated to be the first account in cookies.
  identity_test_env()->RevokeSyncConsent();
  base::RunLoop().RunUntilIdle();

  // Primary account is cleared, but unconsented account is not.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // On Lacros, the UPA does not change on sync consent revoked.
      second_account,
#else
      is_signout_allowed() ? first_account : second_account,
#endif
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ(1U, observer().events().size());
#else
  EXPECT_EQ(is_signout_allowed() ? 2U : 1U, observer().events().size());
#endif
  event = observer().events()[0];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kNone,
            event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(second_account, event.GetPreviousState().primary_account);
  EXPECT_EQ(second_account, event.GetCurrentState().primary_account);

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (is_signout_allowed()) {
  event = observer().events()[1];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kNone,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
            event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(second_account, event.GetPreviousState().primary_account);
  EXPECT_EQ(first_account, event.GetCurrentState().primary_account);
  }
#endif
}

TEST_P(SigninManagerTest, UnconsentedPrimaryAccountUpdatedOnHandleDestroyed) {
  base::HistogramTester histogram_tester;
  AccountInfo first_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo second_account =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);
  identity_test_env()->SetCookieAccounts(
      {{first_account.email, first_account.gaia},
       {second_account.email, second_account.gaia}});
  ASSERT_EQ(first_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  ExpectUnconsentedPrimaryAccountSetEvent(first_account);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);

  std::unique_ptr<AccountSelectionInProgressHandle> handle =
      signin_manager_->CreateAccountSelectionInProgressHandle();
  ASSERT_TRUE(handle);

  // Set the primary account to the second account in cookies. This simulates
  // that the user chose the second account as the to-be-synced account.
  // The unconsented primary account should be updated.
  identity_test_env()->SetPrimaryAccount(second_account.email,
                                         ConsentLevel::kSignin);
  InitializeSignoutDecision();
  EXPECT_EQ(second_account,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  // TODO(crbug.com/1261772): The change should be logged in some way.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectTotalCount("Signin.SignOut.Completed", 0);
  observer().Reset();

  // Release the handle. The unconsented primary account should be updated to be
  // the first account in cookies.
  handle.reset();
  ASSERT_FALSE(handle);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // On Lacros, the UPA is not computed based on cookies, so it won't be
      // automatically reset to the "first" account.
      second_account,
#else
      is_signout_allowed() ? first_account : second_account,
#endif
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ(0U, observer().events().size());
#else
  if (is_signout_allowed()) {
    ExpectUnconsentedPrimaryAccountChangedEvent(second_account, first_account);
  } else {
    EXPECT_EQ(0U, observer().events().size());
  }
#endif
  // TODO(crbug.com/1261772): The change should be logged in some way.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectTotalCount("Signin.SignOut.Completed", 0);
}

TEST_P(SigninManagerTest, ClearPrimaryAccountAndSignOut) {
  AccountInfo account = MakeSyncAccountAvailableWithCookies(kTestEmail);
  ExpectSyncPrimaryAccountSetEvent(account);

  identity_test_env()->ClearPrimaryAccount();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, observer().events().size());
  auto event = observer().events()[0];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(account, event.GetPreviousState().primary_account);
  EXPECT_TRUE(event.GetCurrentState().primary_account.IsEmpty());
}

// Disabling `kSigninAllowed` is not supported on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(SigninManagerTest,
       UnconsentedPrimaryAccountClearedWhenSigninDisallowed) {
  // Prerequisite: add an unconsented primary account.
  AccountInfo account = MakeAccountAvailableWithCookies(kTestEmail);
  ExpectUnconsentedPrimaryAccountSetEvent(account);

  prefs_.SetBoolean(prefs::kSigninAllowed, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  EXPECT_EQ(1U, observer().events().size());
  auto event = observer().events()[0];
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(account, event.GetPreviousState().primary_account);
  EXPECT_TRUE(event.GetCurrentState().primary_account.IsEmpty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

INSTANTIATE_TEST_SUITE_P(SignoutAllowed, SigninManagerTest, ::testing::Bool());

}  // namespace signin
