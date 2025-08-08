// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_

#include <string>

#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace signin {
enum class ConsentLevel;
class IdentityManager;
}

namespace password_manager {

enum class SyncState;
struct PasswordForm;

namespace sync_util {

// Uses `sync_service` and `identity_manager` to determine whether the user is
// signed in with sync-the-feature turned on, and if so, return the e-mail
// representing the account for which sync is on. Returns an empty string
// otherwise (which includes the sync-off case even if account passwords are
// on).
// TODO(crbug.com/1462552): Remove this function once IsSyncFeatureEnabled() is
// fully deprecated, see ConsentLevel::kSync documentation for details.
std::string GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager);

// Returns true if `url` is google.com domain and `username` corresponds to the
// account specified by GetAccountEmailIfSyncFeatureEnabledIncludingPasswords.
// Returns false if GetAccountEmailIfSyncFeatureEnabledIncludingPasswords does
// not specify any account.
bool IsSyncAccountCredential(const GURL& url,
                             const std::u16string& username,
                             const syncer::SyncService* sync_service,
                             const signin::IdentityManager* identity_manager);

// If `username` matches the signed-in account.
bool IsSyncAccountEmail(const std::string& username,
                        const signin::IdentityManager* identity_manager,
                        signin::ConsentLevel consent_level);

// If `signon_realm` matches Gaia signon realm.
bool IsGaiaCredentialPage(const std::string& signon_realm);

// If `form`'s origin matches enterprise login URL or enterprise change password
// URL.
bool ShouldSaveEnterprisePasswordHash(const PasswordForm& form,
                                      const PrefService& prefs);

// If the user turned sync-the-feature on and syncing of passwords is enabled in
// settings.
//
// IMPORTANT NOTE: this function returns false for signed-in-not-syncing users,
// even if account passwords are enabled. On some platforms, e.g. iOS, this can
// be the majority of users (eventually all), so please avoid integrating with
// this function if possible.
// TODO(crbug.com/1462552): Remove this function once IsSyncFeatureEnabled() is
// fully deprecated, see ConsentLevel::kSync documentation for details.
bool IsSyncFeatureEnabledIncludingPasswords(
    const syncer::SyncService* sync_service);

// Returns whether sync-the-feature is on (i.e. configured to be on), active
// (i.e. initialized and not paused) and including syncing of passwords.
//
// IMPORTANT NOTE: this function returns false for signed-in-not-syncing users,
// even if account passwords are enabled and active. On some platforms, e.g.
// iOS, this can be the majority of users (eventually all), so please avoid
// integrating with this function if possible.
// TODO(crbug.com/1462552): Remove this function once IsSyncFeatureEnabled()/
// IsSyncFeatureActive() is fully deprecated, see ConsentLevel::kSync
// documentation for details.
bool IsSyncFeatureActiveIncludingPasswords(
    const syncer::SyncService* sync_service);

// Active syncing account if one exists. If password sync is disabled
// absl::nullopt will be returned.
absl::optional<std::string> GetSyncingAccount(
    const syncer::SyncService* sync_service);

// Returns the account where passwords are being saved, or nullopt if passwords
// are being saved only locally. In practice, this returns a non-empty
// value if the user is syncing or signed in and opted in to account storage.
absl::optional<std::string> GetAccountForSaving(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Reports whether and how passwords are currently synced. In particular, for a
// null `sync_service` returns NOT_SYNCING.
password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service);

}  // namespace sync_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_
