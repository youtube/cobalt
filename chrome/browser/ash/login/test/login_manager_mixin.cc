// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/login_manager_mixin.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace {

// Ensure LoginManagerMixin is only created once.
bool g_instance_created = false;

constexpr char kGmailDomain[] = "@gmail.com";
constexpr char kManagedDomain[] = "@example.com";

void AppendUsers(LoginManagerMixin::UserList* users,
                 const std::string& domain,
                 int n,
                 CryptohomeMixin* cryptohome_mixin) {
  int num = users->size();
  for (int i = 0; i < n; ++i, ++num) {
    const std::string email = "test_user_" + base::NumberToString(num) + domain;
    const std::string gaia_id = base::NumberToString(num) + "111111111";
    const AccountId account_id = AccountId::FromUserEmailGaiaId(email, gaia_id);
    users->push_back(LoginManagerMixin::TestUserInfo(account_id));

    if (cryptohome_mixin != nullptr) {
      cryptohome_mixin->MarkUserAsExisting(account_id);
    }
  }
}

}  // namespace

// static
UserContext LoginManagerMixin::CreateDefaultUserContext(
    const TestUserInfo& user_info) {
  UserContext user_context(user_info.user_type, user_info.account_id);
  user_context.SetKey(Key("password"));
  return user_context;
}

void LoginManagerMixin::AppendRegularUsers(int n) {
  AppendUsers(&initial_users_, kGmailDomain, n, cryptohome_mixin_);
}

void LoginManagerMixin::AppendManagedUsers(int n) {
  AppendUsers(&initial_users_, kManagedDomain, n, cryptohome_mixin_);
}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host)
    : LoginManagerMixin(host, UserList()) {}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                                     const UserList& initial_users)
    : LoginManagerMixin(host, initial_users, nullptr) {}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                                     const UserList& initial_users,
                                     FakeGaiaMixin* gaia_mixin)
    : LoginManagerMixin(host, initial_users, gaia_mixin, nullptr) {}

LoginManagerMixin::LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                                     const UserList& initial_users,
                                     FakeGaiaMixin* gaia_mixin,
                                     CryptohomeMixin* cryptohome_mixin)
    : InProcessBrowserTestMixin(host),
      initial_users_(initial_users),
      local_state_mixin_(host, this),
      fake_gaia_mixin_(gaia_mixin),
      cryptohome_mixin_(cryptohome_mixin) {
  DCHECK(!g_instance_created);
  g_instance_created = true;

  if (cryptohome_mixin != nullptr) {
    for (const auto& user : initial_users_) {
      cryptohome_mixin_->MarkUserAsExisting(user.account_id);
    }
  }
}

LoginManagerMixin::~LoginManagerMixin() {
  g_instance_created = false;
}

void LoginManagerMixin::SetDefaultLoginSwitches(
    const std::vector<test::SessionFlagsManager::Switch>& switches) {
  session_flags_manager_.SetDefaultLoginSwitches(switches);
}

bool LoginManagerMixin::SetUpUserDataDirectory() {
  if (session_restore_enabled_)
    session_flags_manager_.SetUpSessionRestore();
  session_flags_manager_.AppendSwitchesToCommandLine(
      base::CommandLine::ForCurrentProcess());
  return true;
}

void LoginManagerMixin::SetUpLocalState() {
  for (const auto& user : initial_users_) {
    ScopedListPrefUpdate users_pref(g_browser_process->local_state(),
                                    "LoggedInUsers");
    base::Value email_value(user.account_id.GetUserEmail());
    if (!base::Contains(users_pref.Get(), email_value))
      users_pref->Append(std::move(email_value));

    ScopedDictPrefUpdate user_type_update(g_browser_process->local_state(),
                                          "UserType");
    user_type_update->Set(user.account_id.GetAccountIdKey(),
                          static_cast<int>(user.user_type));

    ScopedDictPrefUpdate user_token_update(g_browser_process->local_state(),
                                           "OAuthTokenStatus");
    user_token_update->Set(user.account_id.GetUserEmail(),
                           static_cast<int>(user.token_status));

    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.UpdateId(user.account_id);

    if (user.user_type == user_manager::USER_TYPE_CHILD) {
      known_user.SetProfileRequiresPolicy(
          user.account_id,
          user_manager::ProfileRequiresPolicy::kPolicyRequired);
    }

    if (base::EndsWith(kManagedDomain, gaia::ExtractDomainName(
                                           user.account_id.GetUserEmail()))) {
      known_user.SetIsEnterpriseManaged(user.account_id, true);
    }
  }

  StartupUtils::MarkOobeCompleted();
}

void LoginManagerMixin::SetUpOnMainThread() {
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(
      should_obtain_handles_);
}

void LoginManagerMixin::TearDownOnMainThread() {
  session_flags_manager_.Finalize();
}

void LoginManagerMixin::AttemptLoginUsingFakeDataAuthClient(
    const UserContext& user_context) {
  ExistingUserController::current_controller()->Login(user_context,
                                                      SigninSpecifics());
  if (skip_post_login_screens_ && ash::WizardController::default_controller()) {
    ash::WizardController::default_controller()
        ->SkipPostLoginScreensForTesting();
  }
}

void LoginManagerMixin::AttemptLoginUsingAuthenticator(
    const UserContext& user_context,
    std::unique_ptr<StubAuthenticatorBuilder> authenticator_builder) {
  test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
      .InjectAuthenticatorBuilder(std::move(authenticator_builder));
  ExistingUserController::current_controller()->Login(user_context,
                                                      SigninSpecifics());
  if (skip_post_login_screens_ && WizardController::default_controller())
    WizardController::default_controller()->SkipPostLoginScreensForTesting();
}

void LoginManagerMixin::WaitForActiveSession() {
  SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();
}

bool LoginManagerMixin::LoginAndWaitForActiveSession(
    const UserContext& user_context) {
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  WaitForActiveSession();

  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  return active_user &&
         active_user->GetAccountId() == user_context.GetAccountId();
}

void LoginManagerMixin::LoginWithDefaultContext(
    const TestUserInfo& user_info,
    bool wait_for_profile_prepared) {
  UserContext user_context = CreateDefaultUserContext(user_info);
  test::ProfilePreparedWaiter profile_prepared(user_info.account_id);
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));

  if (wait_for_profile_prepared) {
    profile_prepared.Wait();
  }
}

void LoginManagerMixin::LoginAsNewRegularUser(
    absl::optional<UserContext> user_context) {
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);
  test::WaitForOobeJSReady();
  ASSERT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());
  if (!user_context.has_value()) {
    user_context = CreateDefaultUserContext(TestUserInfo(
        AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)));
  }

  test::ProfilePreparedWaiter profile_prepared(user_context->GetAccountId());
  AttemptLoginUsingAuthenticator(
      *user_context, std::make_unique<StubAuthenticatorBuilder>(*user_context));
  profile_prepared.Wait();
}

void LoginManagerMixin::LoginAsNewChildUser() {
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);
  test::WaitForOobeJSReady();
  ASSERT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());
  TestUserInfo test_child_user_(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      user_manager::USER_TYPE_CHILD);
  UserContext user_context = CreateDefaultUserContext(test_child_user_);
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  ASSERT_TRUE(fake_gaia_mixin_) << "Pass FakeGaiaMixin into constructor";
  fake_gaia_mixin_->SetupFakeGaiaForChildUser(
      test_child_user_.account_id.GetUserEmail(),
      test_child_user_.account_id.GetGaiaId(), FakeGaiaMixin::kFakeRefreshToken,
      false /*issue_any_scope_token*/);
  test::ProfilePreparedWaiter profile_prepared(user_context.GetAccountId());
  AttemptLoginUsingAuthenticator(
      user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  profile_prepared.Wait();
}

void LoginManagerMixin::SkipPostLoginScreens() {
  skip_post_login_screens_ = true;
  if (WizardController::default_controller()) {
    WizardController::default_controller()->SkipPostLoginScreensForTesting();
  }
}

}  // namespace ash
