// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/extensions_activity.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace trusted_vault {
class TrustedVaultClient;
}

namespace syncer {

class SyncEngineFactory;
class SyncInvalidationsService;
class TrustedVaultAutoUpgradeSyntheticFieldTrialGroup;

// Interface for clients of the Sync API to plumb through necessary dependent
// components. This interface is purely for abstracting dependencies, and
// should not contain any non-trivial functional logic.
//
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
class SyncClient {
 public:
  SyncClient() = default;
  SyncClient(const SyncClient&) = delete;
  SyncClient& operator=(const SyncClient&) = delete;
  virtual ~SyncClient() = default;

  // Returns the current profile's preference service.
  virtual PrefService* GetPrefService() = 0;

  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the path to the folder used for storing the local sync database.
  // It is only used when sync is running against a local backend.
  virtual base::FilePath GetLocalSyncBackendFolder() = 0;

  virtual SyncInvalidationsService* GetSyncInvalidationsService() = 0;
  virtual trusted_vault::TrustedVaultClient* GetTrustedVaultClient() = 0;
  virtual scoped_refptr<ExtensionsActivity> GetExtensionsActivity() = 0;

  // Returns the current SyncEngineFactory instance.
  virtual SyncEngineFactory* GetSyncEngineFactory() = 0;

  // Returns whether custom passphrase is allowed for the current user.
  virtual bool IsCustomPassphraseAllowed() = 0;

  // Necessary but not sufficient condition for password sync to be enabled,
  // i.e. it influences the value of SyncUserSettings::GetSelectedTypes().
  // TODO(crbug.com/328190573): Remove this and SetPasswordSyncAllowedChangeCb()
  // below when the local UPM migration is gone.
  virtual bool IsPasswordSyncAllowed() = 0;

  // Causes `cb` to be invoked whenever the value of IsPasswordSyncAllowed()
  // changes. Spurious invocations can occur too. This method must be called at
  // most once.
  virtual void SetPasswordSyncAllowedChangeCb(
      const base::RepeatingClosure& cb) = 0;

  // Registers synthetic field trials corresponding to autoupgrading users to
  // trusted vault passphrase type. `group` must be valid. Must be invoked at
  // most once.
  virtual void RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_
