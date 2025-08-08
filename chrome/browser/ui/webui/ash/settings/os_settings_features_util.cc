// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"

#include "ash/components/arc/arc_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::settings {

bool IsGuestModeActive() {
  return ash::ChromeUserManager::Get()->IsLoggedInAsGuest() ||
         ash::ChromeUserManager::Get()->IsLoggedInAsManagedGuestSession();
}

bool IsChildUser() {
  return ash::ChromeUserManager::Get()->IsLoggedInAsChildUser();
}

bool IsDeviceEnterpriseManaged() {
  return ash::ChromeUserManager::Get()->IsEnterpriseManaged();
}

bool IsPowerwashAllowed() {
  return !IsDeviceEnterpriseManaged() && !IsGuestModeActive() && !IsChildUser();
}

bool ShouldShowParentalControlSettings(const Profile* profile) {
  // Not shown for secondary users.
  if (profile != ProfileManager::GetPrimaryUserProfile())
    return false;

  // Also not shown for guest sessions.
  if (profile->IsGuestSession())
    return false;

  return profile->IsChild() ||
         !profile->GetProfilePolicyConnector()->IsManaged();
}

bool IsExternalStorageEnabled(const Profile* profile) {
  return base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature) &&
         arc::IsArcPlayStoreEnabledForProfile(profile);
}

bool IsAppRestoreAvailableForProfile(const Profile* profile) {
  return full_restore::FullRestoreServiceFactory::
      IsFullRestoreAvailableForProfile(profile);
}

}  // namespace ash::settings
