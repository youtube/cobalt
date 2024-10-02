// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

ChromeUserManager::ChromeUserManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserManagerBase(
          std::move(task_runner),
          g_browser_process ? g_browser_process->local_state() : nullptr),
      reporting_user_tracker_(
          g_browser_process ? g_browser_process->local_state() : nullptr) {
  reporting_user_tracker_observation_.Observe(this);
}

ChromeUserManager::~ChromeUserManager() = default;

// static
void ChromeUserManager::RegisterPrefs(PrefRegistrySimple* registry) {
  UserManagerBase::RegisterPrefs(registry);

  registry->RegisterListPref(::prefs::kReportingUsers);
}

bool ChromeUserManager::IsCurrentUserNew() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceFirstRunUI))
    return true;

  return UserManagerBase::IsCurrentUserNew();
}

void ChromeUserManager::UpdateLoginState(const user_manager::User* active_user,
                                         const user_manager::User* primary_user,
                                         bool is_current_user_owner) const {
  if (!LoginState::IsInitialized())
    return;  // LoginState may be uninitialized in tests.

  LoginState::LoggedInState logged_in_state;
  LoginState::LoggedInUserType logged_in_user_type;
  if (active_user) {
    logged_in_state = LoginState::LOGGED_IN_ACTIVE;
    logged_in_user_type =
        GetLoggedInUserType(*active_user, is_current_user_owner);
  } else {
    logged_in_state = LoginState::LOGGED_IN_NONE;
    logged_in_user_type = LoginState::LOGGED_IN_USER_NONE;
  }

  if (primary_user) {
    LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, logged_in_user_type, primary_user->username_hash());
  } else {
    LoginState::Get()->SetLoggedInState(logged_in_state, logged_in_user_type);
  }
}

bool ChromeUserManager::GetPlatformKnownUserId(
    const std::string& user_email,
    AccountId* out_account_id) const {
  if (user_email == user_manager::kStubUserEmail) {
    *out_account_id = user_manager::StubAccountId();
    return true;
  }

  if (user_email == user_manager::kStubAdUserEmail) {
    *out_account_id = user_manager::StubAdAccountId();
    return true;
  }

  if (user_email == user_manager::kGuestUserName) {
    *out_account_id = user_manager::GuestAccountId();
    return true;
  }

  return false;
}

LoginState::LoggedInUserType ChromeUserManager::GetLoggedInUserType(
    const user_manager::User& active_user,
    bool is_current_user_owner) const {
  if (is_current_user_owner)
    return LoginState::LOGGED_IN_USER_OWNER;

  switch (active_user.GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return LoginState::LOGGED_IN_USER_REGULAR;
    case user_manager::USER_TYPE_GUEST:
      return LoginState::LOGGED_IN_USER_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::USER_TYPE_CHILD:
      return LoginState::LOGGED_IN_USER_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      // NOTE(olsen) There's no LOGGED_IN_USER_ACTIVE_DIRECTORY - is it needed?
      return LoginState::LOGGED_IN_USER_REGULAR;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return LoginState::LOGGED_IN_USER_KIOSK;
    case user_manager::NUM_USER_TYPES:
      break;  // Go to invalid-type handling code.
      // Since there is no default, the compiler warns about unhandled types.
  }
  NOTREACHED() << "Invalid type for active user: " << active_user.GetType();
  return LoginState::LOGGED_IN_USER_REGULAR;
}

// static
ChromeUserManager* ChromeUserManager::Get() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  return user_manager ? static_cast<ChromeUserManager*>(user_manager) : nullptr;
}

bool ChromeUserManager::ShouldReportUser(const std::string& user_id) const {
  return reporting_user_tracker_.ShouldReportUser(user_id);
}

}  // namespace ash
