// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/test_session_controller_client.h"

#include <ranges>
#include <string>

#include "ash/login/login_screen_controller.h"
#include "ash/login_status.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Returns the "canonicalized" email from a given |email| address. Note
// production code should use gaia::CanonicalizeEmail. This is used in tests
// without introducing dependency on google_api.
std::string GetUserIdFromEmail(std::string_view email) {
  return base::ToLowerASCII(email);
}

}  // namespace

TestSessionControllerClient::TestSessionControllerClient(
    SessionControllerImpl* controller,
    TestPrefServiceProvider* prefs_provider,
    bool create_signin_pref_service)
    : controller_(controller), prefs_provider_(prefs_provider) {
  CHECK(controller_);
  CHECK(prefs_provider_);
  Reset();

  if (create_signin_pref_service) {
    prefs_provider_->CreateSigninPrefsIfNeeded();
  }
}

TestSessionControllerClient::~TestSessionControllerClient() = default;

void TestSessionControllerClient::InitializeAndSetClient() {
  session_info_.can_lock_screen = controller_->CanLockScreen();
  session_info_.should_lock_screen_automatically =
      controller_->ShouldLockScreenAutomatically();
  session_info_.add_user_session_policy = controller_->GetAddUserPolicy();
  session_info_.state = controller_->GetSessionState();

  controller_->SetClient(this);
  controller_->EnsureSigninScreenPrefService();
}

void TestSessionControllerClient::Reset() {
  session_info_.can_lock_screen = true;
  session_info_.should_lock_screen_automatically = false;
  session_info_.add_user_session_policy = AddUserSessionPolicy::ALLOWED;
  session_info_.state = session_manager::SessionState::LOGIN_PRIMARY;
  first_session_ready_fired_ = false;
  // Emulate using the same pref service when re-login.
  reuse_pref_service_ = true;

  controller_->ClearUserSessionsForTest();
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::SetCanLockScreen(bool can_lock) {
  session_info_.can_lock_screen = can_lock;
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::SetShouldLockScreenAutomatically(
    bool should_lock) {
  session_info_.should_lock_screen_automatically = should_lock;
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::SetAddUserSessionPolicy(
    AddUserSessionPolicy policy) {
  session_info_.add_user_session_policy = policy;
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::SetSessionState(
    session_manager::SessionState state) {
  session_info_.state = state;
  controller_->SetSessionInfo(session_info_);

  MaybeNotifyFirstSessionReady();
}

void TestSessionControllerClient::SetIsRunningInAppMode(bool app_mode) {
  session_info_.is_running_in_app_mode = app_mode;
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::SetIsDemoSession() {
  session_info_.is_demo_session = true;
  controller_->SetSessionInfo(session_info_);
}

void TestSessionControllerClient::AddUserSession(
    std::string_view display_email,
    user_manager::UserType user_type,
    std::unique_ptr<PrefService> pref_service,
    bool is_new_profile,
    const std::string& given_name,
    bool is_account_managed) {
  auto account_id = AccountId::FromUserEmail(
      use_lower_case_user_id_ ? GetUserIdFromEmail(display_email)
                              : display_email);
  AddUserSession(account_id, display_email, user_type, std::move(pref_service),
                 is_new_profile, given_name, is_account_managed);
}

void TestSessionControllerClient::AddUserSession(
    const AccountId& account_id,
    std::string_view display_email,
    user_manager::UserType user_type,
    std::unique_ptr<PrefService> pref_service,
    bool is_new_profile,
    const std::string& given_name,
    bool is_account_managed) {
  // Set is_ephemeral in user_info to true if the user type is guest or public
  // account.
  bool is_ephemeral = user_type == user_manager::UserType::kGuest ||
                      user_type == user_manager::UserType::kPublicAccount;

  if (pref_service) {
    CHECK(!controller_->GetUserPrefServiceForUser(account_id));
    prefs_provider_->SetUserPrefs(account_id, std::move(pref_service));
  } else if (!provide_pref_service_) {
    CHECK(GetUserPrefService(account_id));
  } else if (!GetUserPrefService(account_id)) {
    prefs_provider_->SetUserPrefs(
        account_id, TestPrefServiceProvider::CreateUserPrefServiceSimple());
  } else {
    CHECK(reuse_pref_service_);
    CHECK(GetUserPrefService(account_id));
  }

  UserSession session;
  session.session_id = ++fake_session_id_;
  session.user_info.type = user_type;
  session.user_info.account_id = account_id;
  session.user_info.display_name = "Über tray Über tray Über tray Über tray";
  session.user_info.display_email = display_email;
  session.user_info.is_ephemeral = is_ephemeral;
  session.user_info.is_new_profile = is_new_profile;
  session.user_info.given_name = given_name;
  session.user_info.has_gaia_account =
      account_id.GetAccountType() == AccountType::GOOGLE &&
      !account_id.GetGaiaId().empty();
  session.user_info.is_managed = is_account_managed;
  controller_->UpdateUserSession(std::move(session));

  MaybeNotifyFirstSessionReady();
}

void TestSessionControllerClient::LockScreen() {
  RequestLockScreen();
  FlushForTest();
}

void TestSessionControllerClient::UnlockScreen() {
  RequestHideLockScreen();
}

void TestSessionControllerClient::FlushForTest() {
  base::RunLoop().RunUntilIdle();
}

void TestSessionControllerClient::SetSigninScreenPrefService(
    std::unique_ptr<PrefService> pref_service) {
  prefs_provider_->SetSigninPrefs(std::move(pref_service));
  controller_->OnSigninScreenPrefServiceInitialized(
      prefs_provider_->GetSigninPrefs());
}

void TestSessionControllerClient::SetUnownedUserPrefService(
    const AccountId& account_id,
    raw_ptr<PrefService> unowned_pref_service) {
  CHECK(!controller_->GetUserPrefServiceForUser(account_id));

  prefs_provider_->SetUnownedUserPrefs(account_id,
                                       std::move(unowned_pref_service));
}

void TestSessionControllerClient::RequestLockScreen() {
  if (should_show_lock_screen_) {
    // The lock screen can't be shown without a wallpaper.
    Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
    Shell::Get()->login_screen_controller()->ShowLockScreen();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestSessionControllerClient::SetSessionState,
                                weak_ptr_factory_.GetWeakPtr(),
                                session_manager::SessionState::LOCKED));
}

void TestSessionControllerClient::RequestHideLockScreen() {
  ++request_hide_lock_screen_count_;
  SetSessionState(session_manager::SessionState::ACTIVE);
}

void TestSessionControllerClient::RequestSignOut() {
  Reset();
  ++request_sign_out_count_;
}

void TestSessionControllerClient::RequestRestartForUpdate() {
  ++request_restart_for_update_count_;
}

void TestSessionControllerClient::AttemptRestartChrome() {
  ++attempt_restart_chrome_count_;
}

void TestSessionControllerClient::SwitchActiveUser(
    const AccountId& account_id) {
  controller_->CanSwitchActiveUser(
      base::BindOnce(&TestSessionControllerClient::DoSwitchUser,
                     weak_ptr_factory_.GetWeakPtr(), account_id));
}

void TestSessionControllerClient::CycleActiveUser(
    CycleUserDirection direction) {
  DCHECK_GT(controller_->NumberOfLoggedInUsers(), 0);

  const SessionControllerImpl::UserSessions& sessions =
      controller_->GetUserSessions();
  const size_t session_count = sessions.size();

  // The following code depends on that |fake_session_id_| is used to generate
  // session ids.
  uint32_t session_id = sessions[0]->session_id;
  if (direction == CycleUserDirection::NEXT) {
    ++session_id;
  } else if (direction == CycleUserDirection::PREVIOUS) {
    DCHECK_GT(session_id, 0u);
    --session_id;
  } else {
    LOG(ERROR) << "Invalid cycle user direction="
               << static_cast<int>(direction);
    return;
  }

  // Valid range of an session id is [1, session_count].
  if (session_id < 1u)
    session_id = session_count;
  if (session_id > session_count)
    session_id = 1u;

  // Maps session id to AccountId and call SwitchActiveUser.
  auto it = std::ranges::find_if(
      sessions, [session_id](const std::unique_ptr<UserSession>& session) {
        return session && session->session_id == session_id;
      });
  if (it == sessions.end()) {
    NOTREACHED();
  }

  SwitchActiveUser((*it)->user_info.account_id);
}

void TestSessionControllerClient::ShowMultiProfileLogin() {
  SetSessionState(session_manager::SessionState::LOGIN_SECONDARY);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.bounds = gfx::Rect(0, 0, 400, 300);
  params.context = Shell::GetPrimaryRootWindow();

  multi_profile_login_widget_ = std::make_unique<views::Widget>();
  multi_profile_login_widget_->Init(std::move(params));
  multi_profile_login_widget_->Show();
}

void TestSessionControllerClient::EmitAshInitialized() {}

PrefService* TestSessionControllerClient::GetSigninScreenPrefService() {
  return prefs_provider_->GetSigninPrefs();
}

PrefService* TestSessionControllerClient::GetUserPrefService(
    const AccountId& account_id) {
  return prefs_provider_->GetUserPrefs(account_id);
}

base::FilePath TestSessionControllerClient::GetProfilePath(
    const AccountId& account_id) {
  return base::FilePath("/profile/path").Append(account_id.GetUserEmail());
}

std::tuple<bool, bool> TestSessionControllerClient::IsEligibleForSeaPen(
    const AccountId& account_id) {
  return is_eligible_for_background_replace_;
}

std::optional<int> TestSessionControllerClient::GetExistingUsersCount() const {
  return existing_users_count_;
}

int TestSessionControllerClient::NumberOfLoggedInUsers() const {
  // This should be migrated to GetExistingUserCount when
  // TestSessionControllerImpl is removed.
  return controller_->NumberOfLoggedInUsers();
}

void TestSessionControllerClient::DoSwitchUser(const AccountId& account_id,
                                               bool switch_user) {
  if (!switch_user)
    return;

  std::vector<uint32_t> session_order;
  session_order.reserve(controller_->GetUserSessions().size());

  for (const auto& user_session : controller_->GetUserSessions()) {
    if (user_session->user_info.account_id == account_id) {
      session_order.insert(session_order.begin(), user_session->session_id);
    } else {
      session_order.push_back(user_session->session_id);
    }
  }

  controller_->SetUserSessionOrder(session_order);
}

void TestSessionControllerClient::MaybeNotifyFirstSessionReady() {
  if (!first_session_ready_fired_ &&
      controller_->IsActiveUserSessionStarted() &&
      session_info_.state == session_manager::SessionState::ACTIVE) {
    first_session_ready_fired_ = true;
    controller_->NotifyFirstSessionReady();
  }
}

void TestSessionControllerClient::NotifyUserPrefServiceInitialized(
    const AccountId& account_id) {
  CHECK(controller_->IsActiveUserSessionStarted());
  controller_->OnProfilePrefServiceInitialized(
      account_id, prefs_provider_->GetUserPrefs(account_id));
}

}  // namespace ash
