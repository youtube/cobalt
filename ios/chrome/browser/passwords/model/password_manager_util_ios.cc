// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/password_manager_util_ios.h"

#include "base/notreached.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/sync/service/sync_service.h"

namespace password_manager_util {

bool IsSavingPasswordsToAccountWithNormalEncryption(
    const syncer::SyncService* sync_service) {
  switch (password_manager::sync_util::GetPasswordSyncState(sync_service)) {
    case password_manager::SyncState::kSyncingNormalEncryption:
    case password_manager::SyncState::kAccountPasswordsActiveNormalEncryption:
      return true;
    case password_manager::SyncState::kNotSyncing:
    case password_manager::SyncState::kSyncingWithCustomPassphrase:
    case password_manager::SyncState::
        kAccountPasswordsActiveWithCustomPassphrase:
      return false;
  }
  NOTREACHED_NORETURN();
}

}  // namespace password_manager_util
