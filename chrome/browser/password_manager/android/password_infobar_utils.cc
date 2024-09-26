// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

AccountInfo GetAccountInfoForPasswordMessages(Profile* profile) {
  DCHECK(profile);

  if (!password_bubble_experiment::HasChosenToSyncPasswords(
          SyncServiceFactory::GetForProfile(profile))) {
    return AccountInfo();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

}  // namespace password_manager
