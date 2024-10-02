// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_SETTINGS_WATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_SETTINGS_WATCHER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

// Helper class to watch for changes to the password account storage preferences
// such as opt-in state and default storage location.
class PasswordAccountStorageSettingsWatcher
    : public syncer::SyncServiceObserver {
 public:
  // |pref_service| must not be null and must outlive this object.
  // |sync_service| may be null (in incognito profiles or due to a commandline
  // flag), but if non-null must outlive this object.
  // |change_callback| will be invoked whenever any password account storage
  // setting is changed (e.g. the opt-in state or the default storage location).
  PasswordAccountStorageSettingsWatcher(PrefService* pref_service,
                                        syncer::SyncService* sync_service,
                                        base::RepeatingClosure change_callback);
  ~PasswordAccountStorageSettingsWatcher() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
  base::RepeatingClosure change_callback_;

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  PrefChangeRegistrar pref_change_registrar_;
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_SETTINGS_WATCHER_H_
