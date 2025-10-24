// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"

class GaiaId;
class PrefRegistrySimple;
class PrefService;
class PrefValueMap;

namespace sync_pb {
class TrustedVaultAutoUpgradeExperimentGroup;
}  // namespace sync_pb

namespace syncer {

class SyncPrefObserver {
 public:
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) = 0;
  // Called when any of the prefs related to the user's selected data types has
  // changed.
  virtual void OnSelectedTypesPrefChange() = 0;

 protected:
  virtual ~SyncPrefObserver();
};

// SyncPrefs is a helper class that manages getting, setting, and persisting
// global sync preferences. It is not thread-safe, and lives on the UI thread.
class SyncPrefs {
 public:
  enum class SyncAccountState {
    kNotSignedIn = 0,
    // In transport mode.
    kSignedInNotSyncing = 1,
    kSyncing = 2
  };

  // `pref_service` must not be null and must outlive this object.
  explicit SyncPrefs(PrefService* pref_service);

  SyncPrefs(const SyncPrefs&) = delete;
  SyncPrefs& operator=(const SyncPrefs&) = delete;

  ~SyncPrefs();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void AddObserver(SyncPrefObserver* sync_pref_observer);
  void RemoveObserver(SyncPrefObserver* sync_pref_observer);

  // Getters and setters for global sync prefs.

  // First-Setup-Complete is conceptually similar to the user's consent to
  // enable sync-the-feature.
  bool IsInitialSyncFeatureSetupComplete() const;

  // Returns true if the user is considered explicitly signed in to the browser.
  // Returns false if the user is signed out or implicilty signed in (through
  // Dice).
  bool IsExplicitBrowserSignin() const;

  // ChromeOS Ash, IsInitialSyncFeatureSetupComplete() always returns true.
#if !BUILDFLAG(IS_CHROMEOS)
  void SetInitialSyncFeatureSetupComplete();
  void ClearInitialSyncFeatureSetupComplete();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // A boolean representing whether or not configuration has completed at least
  // once since the legacy sync-the-feature was turned on.
  bool IsFirstSyncCompletedInFullSyncMode() const;
  void SetFirstSyncCompletedInFullSyncMode();
  void ClearFirstSyncCompletedInFullSyncMode();

  // Whether the "Sync everything" toggle is enabled. This flag only has an
  // effect if Sync-the-feature is enabled. Note that even if this is true, some
  // types may be disabled e.g. due to enterprise policy.
  bool HasKeepEverythingSynced() const;

  // Returns the set of types that the user has selected to be synced.
  // This is only used for syncing users and takes HasKeepEverythingSynced()
  // into account (i.e. returns "all types").
  // If some types are force-disabled by policy, they will not be included.
  UserSelectableTypeSet GetSelectedTypesForSyncingUser() const;
  // Returns the set of types for the given gaia_id for signed-in users.
  // If some types are force-disabled by policy, they will not be included.
  // Note: this is used for signed-in not syncing users.
  UserSelectableTypeSet GetSelectedTypesForAccount(const GaiaId& gaia_id) const;

  // Returns whether `type` is "managed" i.e. controlled by enterprise policy.
  bool IsTypeManagedByPolicy(UserSelectableType type) const;

  // Returns whether `type` is "managed" i.e. controlled by a custodian (i.e.
  // parent/guardian of a child account).
  bool IsTypeManagedByCustodian(UserSelectableType type) const;

  // Returns true if no value exists for the type pref. Otherwise,
  // returns false.
  bool DoesTypeHaveDefaultValueForAccount(const UserSelectableType type,
                                          const GaiaId& gaia_id);

  // Returns true if the type is disabled; that was either set by a user
  // choice, or when a policy enforces disabling the type. Otherwise, returns
  // false if no value exists for the type pref (default), or if it is enabled.
  // Note: this method checks the actual pref value even if there is a policy
  // applied on the type.
  bool IsTypeDisabledByUserForAccount(const UserSelectableType type,
                                      const GaiaId& gaia_id);

  // Sets the selection state for all `registered_types` and "keep everything
  // synced" flag.
  // `keep_everything_synced` indicates that all current and future types
  // should be synced. If this is set to true, then GetSelectedTypes() will
  // always return UserSelectableTypeSet::All(), even if not all of them are
  // registered or individually marked as selected.
  // Changes are still made to the individual selectable type prefs even if
  // `keep_everything_synced` is true, but won't be visible until it's set to
  // false.
  void SetSelectedTypesForSyncingUser(bool keep_everything_synced,
                                      UserSelectableTypeSet registered_types,
                                      UserSelectableTypeSet selected_types);
  // Used to set user's selected types prefs in Sync-the-transport mode.
  // Note: this is used for signed-in not syncing users.
  void SetSelectedTypeForAccount(UserSelectableType type,
                                 bool is_type_on,
                                 const GaiaId& gaia_id);
  // Used to reset user's selected types prefs in Sync-the-transport mode to its
  // default value. Note: this is used for signed-in not syncing users.
  void ResetSelectedTypeForAccount(UserSelectableType type,
                                   const GaiaId& gaia_id);

  // Used to clear per account prefs for all users *except* the ones in the
  // passed-in `available_gaia_ids`.
  void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<GaiaId>& available_gaia_ids);

#if BUILDFLAG(IS_CHROMEOS)
  // Functions to deal with the Ash-specific state where sync-the-feature is
  // disabled because the user reset sync via dashboard.
  bool IsSyncFeatureDisabledViaDashboard() const;
  void SetSyncFeatureDisabledViaDashboard();
  void ClearSyncFeatureDisabledViaDashboard();

  // Chrome OS provides a separate settings UI surface for sync of OS types,
  // including a separate "Sync All" toggle for OS types.
  bool IsSyncAllOsTypesEnabled() const;
  UserSelectableOsTypeSet GetSelectedOsTypes() const;
  bool IsOsTypeManagedByPolicy(UserSelectableOsType type) const;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet registered_types,
                          UserSelectableOsTypeSet selected_types);

  // Maps `type` to its corresponding preference name.
  static const char* GetPrefNameForOsTypeForTesting(UserSelectableOsType type);

  // Sets `type` as disabled in the given `policy_prefs`, which should
  // correspond to the "managed" (aka policy-controlled) pref store.
  static void SetOsTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                        UserSelectableOsType type);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Whether Sync is disabled on the client for all profiles and accounts.
  bool IsSyncClientDisabledByPolicy() const;

  // Maps `type` to its corresponding preference name.
  static const char* GetPrefNameForTypeForTesting(UserSelectableType type);

  // Sets `type` as disabled in the given `policy_prefs`, which should
  // correspond to the "managed" (aka policy-controlled) pref store.
  static void SetTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                      UserSelectableType type);

  // Sets `type` as disabled in the given `supervised_user_prefs`, which should
  // correspond to the custodian-controlled pref store (i.e. controlled by
  // parent/guardian of a child account).
  static void SetTypeDisabledByCustodian(PrefValueMap* supervised_user_prefs,
                                         UserSelectableType type);

  // Gets the local sync backend enabled state.
  bool IsLocalSyncEnabled() const;

  // The user's passphrase type, determined the first time the engine is
  // successfully initialized.
  std::optional<PassphraseType> GetCachedPassphraseType() const;
  void SetCachedPassphraseType(PassphraseType passphrase_type);
  void ClearCachedPassphraseType();

  // Cached notion of whether or not a persistent auth error exists, useful
  // during profile startup before IdentityManager can determine the
  // authoritative value.
  bool HasCachedPersistentAuthErrorForMetrics() const;
  void SetHasCachedPersistentAuthErrorForMetrics(
      bool has_persistent_auth_error);
  void ClearCachedPersistentAuthErrorForMetrics();

  // The user's TrustedVaultAutoUpgradeExperimentGroup, determined the first
  // time the engine is successfully initialized.
  std::optional<sync_pb::TrustedVaultAutoUpgradeExperimentGroup>
  GetCachedTrustedVaultAutoUpgradeExperimentGroup() const;
  void SetCachedTrustedVaultAutoUpgradeExperimentGroup(
      const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& group);
  void ClearCachedTrustedVaultAutoUpgradeExperimentGroup();

  // The encryption bootstrap token is used for explicit passphrase users
  // (usually custom passphrase) and represents a user-entered passphrase.
  // TODO(crbug.com/40282890): ClearAllEncryptionBootstrapTokens is only needed
  // to clear the gaia-keyed pref on signout for syncing users. It should be
  // removed only when kMigrateSyncingUserToSignedIn is fully rolled-out.
  void ClearAllEncryptionBootstrapTokens();
  // The encryption bootstrap token per account. Used for explicit passphrase
  // users (usually custom passphrase) and represents a user-entered passphrase.
  std::string GetEncryptionBootstrapTokenForAccount(
      const GaiaId& gaia_id) const;
  void SetEncryptionBootstrapTokenForAccount(const std::string& token,
                                             const GaiaId& gaia_id);
  void ClearEncryptionBootstrapTokenForAccount(const GaiaId& gaia_id);

  // Muting mechanism for passphrase prompts, used on Android.
  int GetPassphrasePromptMutedProductVersion() const;
  void SetPassphrasePromptMutedProductVersion(int major_version);
  void ClearPassphrasePromptMutedProductVersion();

  // Migrates any user settings for pre-existing signed-in users, for the
  // feature `kReplaceSyncPromosWithSignInPromos`. For signed-out users or
  // syncing users, no migration is necessary - this also covers new users (or
  // more precisely, new profiles).
  // This should be called early during browser startup.
  // Returns whether the migration ran, i.e. whether any user settings were set.
  bool MaybeMigratePrefsForSyncToSigninPart1(SyncAccountState account_state,
                                             const GaiaId& gaia_id);

  // Second part of the above migration, which depends on the user's passphrase
  // type, which isn't known yet during browser startup. This should be called
  // as soon as the passphrase type is known, and will only do any migration if
  // the above method has flagged that it's necessary.
  // Returns whether the migration ran, i.e. whether any user settings were set.
  bool MaybeMigratePrefsForSyncToSigninPart2(const GaiaId& gaia_id,
                                             bool is_using_explicit_passphrase);

  // Migrates kSyncEncryptionBootstrapToken to the gaia-keyed pref, for the
  // feature `kSyncRememberCustomPassphraseAfterSignout`. This should be called
  // early during browser startup.
  // TODO(crbug.com/325201878): Clean up the migration logic and the old pref.
  void MaybeMigrateCustomPassphrasePref(const GaiaId& gaia_id);

  // Should be called when Sync gets disabled / the user signs out. Clears any
  // temporary state from the above migration.
  void MarkPartialSyncToSigninMigrationFullyDone();

  // Setting to false causes GetSelectedTypesForSyncingUser() and
  // GetSelectedTypesForAccount() to not include passwords, no matter the
  // underlying user settings.
  // TODO(crbug.com/328190573): Remove this when local UPM migration is gone.
  void SetPasswordSyncAllowed(bool allowed);

  static void MigrateAutofillWalletImportEnabledPref(PrefService* pref_service);

  // Copies the global versions of the selected-types prefs (used for syncing
  // users) to the per-account prefs for the given `gaia_id` (used for signed-in
  // non-syncing users). To be used when an existing syncing user is migrated to
  // signed-in.
  static void MigrateGlobalDataTypePrefsToAccount(PrefService* pref_service,
                                                  const GaiaId& gaia_id);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Performs a one-off migration which ensures that, for a user who...
  // ...enabled sync-the-feature, then...
  // ...disabled an autofill data type, then...
  // ...disabled sync-the-feature, then...
  // ...signed-in with the same account (without sync-the-feture), the autofill
  // data type is disabled.
  // Internally this works by reading the global passwords setting and writing
  // it to the account setting for kGoogleServicesLastSyncingGaiaId.
  static void MaybeMigrateAutofillToPerAccountPref(PrefService* pref_service);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 private:
  static void RegisterTypeSelectedPref(PrefRegistrySimple* prefs,
                                       UserSelectableType type);

  static const char* GetPrefNameForType(UserSelectableType type);
#if BUILDFLAG(IS_CHROMEOS)
  static const char* GetPrefNameForOsType(UserSelectableOsType type);
#endif  // BUILDFLAG(IS_CHROMEOS)

  static bool IsTypeSupportedInTransportMode(UserSelectableType type);

  void OnSyncManagedPrefChanged();

  void OnSelectedTypesPrefChanged(const std::string& pref_name);

  // Never null.
  const raw_ptr<PrefService> pref_service_;

  base::ObserverList<SyncPrefObserver>::Unchecked sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management (aka policy).
  BooleanPrefMember pref_sync_managed_;

  PrefChangeRegistrar pref_change_registrar_;

  bool batch_updating_selected_types_ = false;

  bool password_sync_allowed_ = true;

  // Caches the value of the kEnableLocalSyncBackend pref to avoid it flipping
  // during the lifetime of the service.
  const bool local_sync_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_
