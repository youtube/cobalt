// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "build/build_config.h"
#include "components/sync/driver/sync_service_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;
class Profile;
class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.

enum class SyncStatusMessageType {
  // User has not set up sync.
  kPreSynced,
  // We are synced and authenticated to a gmail account.
  kSynced,
  // A sync error (such as invalid credentials) has occurred.
  kSyncError,
  // Same as kSyncError but affecting passwords only.
  kPasswordsOnlySyncError,
};

// The action associated with the sync status in settings.
enum class SyncStatusActionType {
  // No action to take.
  kNoAction,
  // User needs to reauthenticate.
  kReauthenticate,
  // User needs to upgrade the client.
  kUpgradeClient,
  // User needs to enter their passphrase.
  kEnterPassphrase,
  // User needs to go through key retrieval.
  kRetrieveTrustedVaultKeys,
  // User needs to confirm sync settings.
  kConfirmSyncSettings,
};

// Sync errors that should be exposed to the user through the avatar button.
enum AvatarSyncErrorType {
  // Unrecoverable error for managed users.
  kManagedUserUnrecoverableError,
  // Unrecoverable error for regular users.
  kUnrecoverableError,
  // Sync paused (e.g. persistent authentication error).
  kSyncPaused,
  // Out-of-date client error.
  kUpgradeClientError,
  // Sync passphrase error.
  kPassphraseError,
  // Trusted vault keys missing for all sync datatypes (encrypt everything is
  // enabled).
  kTrustedVaultKeyMissingForEverythingError,
  // Trusted vault keys missing for always-encrypted datatypes (passwords).
  kTrustedVaultKeyMissingForPasswordsError,
  // User needs to improve recoverability of the trusted vault (encrypt
  // everything is enabled).
  kTrustedVaultRecoverabilityDegradedForEverythingError,
  // User needs to improve recoverability of the trusted vault (passwords).
  kTrustedVaultRecoverabilityDegradedForPasswordsError,
  // Sync settings dialog not confirmed yet.
  kSettingsUnconfirmedError,
};

struct SyncStatusLabels {
  SyncStatusMessageType message_type;
  int status_label_string_id;
  int button_string_id;
  SyncStatusActionType action_type;
};

// Returns the high-level sync status by querying |sync_service| and
// |identity_manager|.
SyncStatusLabels GetSyncStatusLabels(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    bool is_user_clear_primary_account_allowed);

// Returns the high-level sync status by querying |profile|. This is a
// convenience version of GetSyncStatusLabels that use the |sync_service| and
// |identity_manager| associated to |profile| via their respective factories.
SyncStatusLabels GetSyncStatusLabels(Profile* profile);

// Convenience version of GetSyncStatusLabels for when you're only interested in
// the message type.
SyncStatusMessageType GetSyncStatusMessageType(Profile* profile);

// Gets the error in the sync machinery (if any) that should be exposed to the
// user through the titlebar avatar button. If absl::nullopt is returned, this
// does NOT mean sync-the-feature/sync-the-transport is enabled, simply that
// there's no error. Furthermore, an error may be returned even if only
// sync-the-transport is running. One such case is when the user wishes to run
// an encrypted data type on transport mode and must first go through a reauth.
absl::optional<AvatarSyncErrorType> GetAvatarSyncErrorType(Profile* profile);

// When |error| is present, this returns the string to be shown both as the
// tooltip of the avatar button, and in the profile menu body (the menu opened
// by clicking the avatar button).
std::u16string GetAvatarSyncErrorDescription(AvatarSyncErrorType error,
                                             bool is_sync_feature_enabled);

// Whether sync is currently blocked from starting because the sync
// confirmation dialog hasn't been shown. Note that once the dialog is
// showing (i.e. IsSetupInProgress() is true), this will return false.
bool ShouldRequestSyncConfirmation(const syncer::SyncService* service);

// Returns whether it makes sense to show a Sync passphrase error UI, i.e.
// whether a missing passphrase is preventing Sync from fully starting up.
bool ShouldShowSyncPassphraseError(const syncer::SyncService* service);

// Returns whether missing trusted vault keys is preventing sync from starting
// up encrypted datatypes.
bool ShouldShowSyncKeysMissingError(const syncer::SyncService* sync_service,
                                    const PrefService* pref_service);

// Returns whether user action is required to improve the recoverability of the
// trusted vault.
bool ShouldShowTrustedVaultDegradedRecoverabilityError(
    const syncer::SyncService* sync_service,
    const PrefService* pref_service);

// Opens a tab for the purpose of retrieving the trusted vault keys, which
// usually requires a reauth.
void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger);

// Opens a tab for the purpose of improving the recoverability of the trusted
// vault keys, which usually requires a reauth.
void OpenTabForSyncKeyRecoverabilityDegraded(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger);

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
