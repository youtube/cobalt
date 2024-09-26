// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/profile_helper.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/profiles/browser_context_helper_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_types_ash.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

// static
bool ProfileHelper::enable_profile_to_user_testing = false;
bool ProfileHelper::always_return_primary_user_for_testing = false;

class ProfileHelperImpl : public ProfileHelper {
 public:
  explicit ProfileHelperImpl(
      std::unique_ptr<BrowserContextHelper::Delegate> delegate);
  ~ProfileHelperImpl() override;

  Profile* GetProfileByAccountId(const AccountId& account_id) override;
  Profile* GetProfileByUser(const user_manager::User* user) override;

  const user_manager::User* GetUserByProfile(
      const Profile* profile) const override;
  user_manager::User* GetUserByProfile(Profile* profile) const override;

  void SetProfileToUserMappingForTesting(user_manager::User* user) override;
  void SetUserToProfileMappingForTesting(const user_manager::User* user,
                                         Profile* profile) override;
  void RemoveUserFromListForTesting(const AccountId& account_id) override;

 private:
  std::unique_ptr<BrowserContextHelper> browser_context_helper_;

  // Used for testing by unit tests and FakeUserManager.
  std::map<const user_manager::User*, Profile*> user_to_profile_for_testing_;

  // When this list is not empty GetUserByProfile() will find user that has
  // the same user_id as |profile|->GetProfileName().
  user_manager::UserList user_list_for_testing_;
};

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, public

ProfileHelper::ProfileHelper() = default;

ProfileHelper::~ProfileHelper() = default;

// static
std::unique_ptr<ProfileHelper> ProfileHelper::CreateInstance() {
  return std::make_unique<ProfileHelperImpl>(
      std::make_unique<BrowserContextHelperDelegateImpl>());
}

// static
ProfileHelper* ProfileHelper::Get() {
  return g_browser_process->platform_part()->profile_helper();
}

// static
base::FilePath ProfileHelper::GetProfilePathByUserIdHash(
    const std::string& user_id_hash) {
  return BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
      user_id_hash);
}

// static
base::FilePath ProfileHelper::GetSigninProfileDir() {
  return BrowserContextHelper::Get()->GetSigninBrowserContextPath();
}

// static
Profile* ProfileHelper::GetSigninProfile() {
  return Profile::FromBrowserContext(
      BrowserContextHelper::Get()->DeprecatedGetOrCreateSigninBrowserContext());
}

// static
std::string ProfileHelper::GetUserIdHashFromProfile(const Profile* profile) {
  return BrowserContextHelper::GetUserIdHashFromBrowserContext(
      const_cast<Profile*>(profile));
}

// static
base::FilePath ProfileHelper::GetUserProfileDir(
    const std::string& user_id_hash) {
  return base::FilePath(
      BrowserContextHelper::GetUserBrowserContextDirName(user_id_hash));
}

// static
bool ProfileHelper::IsSigninProfile(const Profile* profile) {
  return ::IsSigninProfile(profile);
}

// static
bool ProfileHelper::IsSigninProfileInitialized() {
  return BrowserContextHelper::Get()->GetSigninBrowserContext();
}

// static
bool ProfileHelper::IsLockScreenAppProfile(const Profile* profile) {
  return ::IsLockScreenAppProfile(profile);
}

// static
base::FilePath ProfileHelper::GetLockScreenAppProfilePath() {
  return BrowserContextHelper::Get()->GetLockScreenAppBrowserContextPath();
}

// static
base::FilePath ProfileHelper::GetLockScreenProfileDir() {
  return BrowserContextHelper::Get()->GetLockScreenBrowserContextPath();
}

// static
Profile* ProfileHelper::GetLockScreenProfile() {
  return Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetLockScreenBrowserContext());
}

// static
bool ProfileHelper::IsLockScreenProfile(const Profile* profile) {
  return ::IsLockScreenProfile(profile);
}

// static
bool ProfileHelper::IsOwnerProfile(const Profile* profile) {
  return user_manager::UserManager::Get()->IsOwnerUser(
      ProfileHelper::Get()->GetUserByProfile(profile));
}

// static
bool ProfileHelper::IsPrimaryProfile(const Profile* profile) {
  return user_manager::UserManager::Get()->IsPrimaryUser(
      ProfileHelper::Get()->GetUserByProfile(profile));
}

// static
bool ProfileHelper::IsEphemeralUserProfile(const Profile* profile) {
  return user_manager::UserManager::Get()->IsEphemeralUser(
      ProfileHelper::Get()->GetUserByProfile(profile));
}

// static
bool ProfileHelper::IsUserProfile(const Profile* profile) {
  return ::IsUserProfile(profile);
}

// static
bool ProfileHelper::IsUserProfilePath(const base::FilePath& profile_path) {
  return ::IsUserProfilePath(profile_path);
}

// static
void ProfileHelper::SetProfileToUserForTestingEnabled(bool enabled) {
  enable_profile_to_user_testing = enabled;
}

// static
void ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(bool value) {
  always_return_primary_user_for_testing = value;
  ProfileHelper::SetProfileToUserForTestingEnabled(value);
}

ProfileHelperImpl::ProfileHelperImpl(
    std::unique_ptr<BrowserContextHelper::Delegate> delegate)
    : browser_context_helper_(
          std::make_unique<BrowserContextHelper>(std::move(delegate))) {}

ProfileHelperImpl::~ProfileHelperImpl() = default;

Profile* ProfileHelperImpl::GetProfileByAccountId(const AccountId& account_id) {
  // TODO(crbug.com/1325210): Remove test injection from here.
  if (!user_to_profile_for_testing_.empty()) {
    const auto* user = user_manager::UserManager::Get()->FindUser(account_id);
    auto it = user_to_profile_for_testing_.find(user);
    if (it != user_to_profile_for_testing_.end()) {
      return it->second;
    }
  }

  return Profile::FromBrowserContext(
      browser_context_helper_->GetBrowserContextByAccountId(account_id));
}

Profile* ProfileHelperImpl::GetProfileByUser(const user_manager::User* user) {
  // TODO(crbug.com/1325210): Remove test injection from here.
  if (!user_to_profile_for_testing_.empty()) {
    auto it = user_to_profile_for_testing_.find(user);
    if (it != user_to_profile_for_testing_.end()) {
      return it->second;
    }
  }

  return Profile::FromBrowserContext(
      browser_context_helper_->GetBrowserContextByUser(user));
}

const user_manager::User* ProfileHelperImpl::GetUserByProfile(
    const Profile* profile) const {
  if (!ProfileHelper::IsUserProfile(profile)) {
    return nullptr;
  }

  // This map is non-empty only in tests.
  if (enable_profile_to_user_testing || !user_list_for_testing_.empty()) {
    if (always_return_primary_user_for_testing)
      return user_manager::UserManager::Get()->GetPrimaryUser();

    const std::string& user_name = profile->GetProfileUserName();
    for (user_manager::UserList::const_iterator it =
             user_list_for_testing_.begin();
         it != user_list_for_testing_.end(); ++it) {
      if ((*it)->GetAccountId().GetUserEmail() == user_name)
        return *it;
    }

    // In case of test setup we should always default to primary user.
    return user_manager::UserManager::Get()->GetPrimaryUser();
  }

  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Special case for non-CrOS tests that do create several profiles
  // and don't really care about mapping to the real user.
  // Without multi-profiles on Chrome OS such tests always got active_user_.
  // Now these tests will specify special flag to continue working.
  // In future those tests can get a proper CrOS configuration i.e. register
  // and login several users if they want to work with an additional profile.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreUserProfileMappingForTests)) {
    return user_manager->GetActiveUser();
  }

  if (const auto* user = browser_context_helper_->GetUserByBrowserContext(
          const_cast<Profile*>(profile));
      user) {
    return user;
  }

  // Many tests do not have their users registered with UserManager and
  // runs here. If |active_user_| matches |profile|, returns it.

  // There's no guard that this is only for testing. Adding metrics here
  // temporarily to make sure we can safely clean up the code.
  // TODO(crbug.com/1325210): Remove the metrics together with the following
  // code refactored.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::UmaHistogramBoolean("Ash.BrowserContext.UnexpectedGetUserByProfile",
                              true);
    // Also taking the stack trace, so we can identify who's the caller on
    // unexpected cases.
    base::debug::DumpWithoutCrashing();
  }

  const user_manager::User* active_user = user_manager->GetActiveUser();
  return active_user &&
                 browser_context_helper_->GetBrowserContextPathByUserIdHash(
                     active_user->username_hash()) == profile->GetPath()
             ? active_user
             : nullptr;
}

user_manager::User* ProfileHelperImpl::GetUserByProfile(
    Profile* profile) const {
  return const_cast<user_manager::User*>(
      GetUserByProfile(static_cast<const Profile*>(profile)));
}

void ProfileHelperImpl::SetProfileToUserMappingForTesting(
    user_manager::User* user) {
  user_list_for_testing_.push_back(user);
}

void ProfileHelperImpl::SetUserToProfileMappingForTesting(
    const user_manager::User* user,
    Profile* profile) {
  DCHECK(user);
  user_to_profile_for_testing_[user] = profile;
}

void ProfileHelperImpl::RemoveUserFromListForTesting(
    const AccountId& account_id) {
  auto it = base::ranges::find(user_list_for_testing_, account_id,
                               &user_manager::User::GetAccountId);
  if (it != user_list_for_testing_.end())
    user_list_for_testing_.erase(it);
}

}  // namespace ash
