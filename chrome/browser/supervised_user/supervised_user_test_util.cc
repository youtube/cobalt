// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user_test_util {

void AddCustodians(Profile* profile) {
  DCHECK(profile->IsChild());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kSupervisedUserCustodianEmail,
                   "test_parent_0@google.com");
  prefs->SetString(prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                   "239029320");

  prefs->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                   "test_parent_1@google.com");
  prefs->SetString(prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
                   "85948533");
}

void SetSupervisedUserExtensionsMayRequestPermissionsPref(Profile* profile,
                                                          bool enabled) {
  // TODO(crbug/1024646): kSupervisedUserExtensionsMayRequestPermissions is
  // currently set indirectly by setting geolocation requests. Update Kids
  // Management server to set a new bit for extension permissions and update
  // this setter function.
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey());
  settings_service->SetLocalSetting(supervised_user::kGeolocationDisabled,
                                    base::Value(!enabled));
  profile->GetPrefs()->SetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, enabled);
}

}  // namespace supervised_user_test_util
