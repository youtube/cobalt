// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace password_manager {

namespace metrics_util {

using IsUsernameChanged = base::StrongAlias<class IsUsernameChangedTag, bool>;
using IsPasswordChanged = base::StrongAlias<class IsPasswordChangedTag, bool>;
using IsPasswordNoteChanged =
    base::StrongAlias<class IsPasswordNoteChangedTag, bool>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordBubble.DisplayDisposition"
enum UIDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING = 1,
  MANUAL_MANAGE_PASSWORDS = 2,
  MANUAL_BLOCKLISTED_OBSOLETE = 3,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION = 4,
  AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE = 5,  // obsolete
  AUTOMATIC_SIGNIN_TOAST = 6,
  MANUAL_WITH_PASSWORD_PENDING_UPDATE = 7,
  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE = 8,
  MANUAL_GENERATED_PASSWORD_CONFIRMATION = 9,
  AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY = 10,
  AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER = 11,
  AUTOMATIC_MOVE_TO_ACCOUNT_STORE = 12,
  MANUAL_BIOMETRIC_AUTHENTICATION_FOR_FILLING = 13,
  AUTOMATIC_BIOMETRIC_AUTHENTICATION_FOR_FILLING = 14,
  AUTOMATIC_BIOMETRIC_AUTHENTICATION_CONFIRMATION = 15,
  NUM_DISPLAY_DISPOSITIONS,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar".
  NO_DIRECT_INTERACTION = 0,
  CLICKED_ACCEPT = 1,
  CLICKED_CANCEL = 2,
  CLICKED_NEVER = 3,
  CLICKED_MANAGE = 4,
  CLICKED_DONE_OBSOLETE = 5,         // obsolete
  CLICKED_UNBLOCKLIST_OBSOLETE = 6,  // obsolete.
  CLICKED_OK_OBSOLETE = 7,           // obsolete
  CLICKED_CREDENTIAL_OBSOLETE = 8,   // obsolete.
  AUTO_SIGNIN_TOAST_TIMEOUT = 9,
  AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE = 10,  // obsolete.
  CLICKED_BRAND_NAME_OBSOLETE = 11,         // obsolete.
  CLICKED_PASSWORDS_DASHBOARD = 12,
  NUM_UI_RESPONSES,
};

// Enum representing the different leak detection dialogs shown to the user.
// Corresponds to LeakDetectionDialogType suffix in histogram_suffixes_list.xml
// and PasswordLeakDetectionDialogType in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// In case of adding a new type, NEXT VALUE: 4.
enum class LeakDialogType {
  // The user is asked to visit the Password Checkup.
  kCheckup = 0,
  // The user is asked to change the password for the current site.
  kChange = 1,
  // The user is asked to visit the Password Checkup and change the password for
  // the current site.
  kCheckupAndChange = 2,
  // This value has been deprecated as part of APC removal.
  // kChangeAutomatically = 3,
  kMaxValue = kCheckupAndChange,
};

// Enum recording the dismissal reason of the data breach dialog which is shown
// in case a credential is reported as leaked. Needs to stay in sync with the
// PasswordLeakDetectionDialogDismissalReason enum in enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// In case of adding a new type, NEXT VALUE: 5.
enum class LeakDialogDismissalReason {
  kNoDirectInteraction = 0,
  kClickedClose = 1,
  kClickedCheckPasswords = 2,
  kClickedOk = 3,
  // This type has been deprecated as part of APC removal.
  // kClickedChangePasswordAutomatically = 4,
  kMaxValue = kClickedOk,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FormDeserializationStatus {
  LOGIN_DATABASE_SUCCESS = 0,
  LOGIN_DATABASE_FAILURE = 1,
  LIBSECRET_SUCCESS = 2,
  LIBSECRET_FAILURE = 3,
  GNOME_SUCCESS = 4,
  GNOME_FAILURE = 5,
  NUM_DESERIALIZATION_STATUSES
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.PasswordSyncState3"
enum class PasswordSyncState {
  kSyncingOk = 0,
  kNotSyncingFailedRead = 1,
  kNotSyncingDuplicateTags = 2,
  kNotSyncingServerError = 3,
  kNotSyncingFailedCleanup = 4,
  kNotSyncingFailedDecryption = 5,
  kNotSyncingFailedAdd = 6,
  kNotSyncingFailedUpdate = 7,
  kNotSyncingFailedMetadataPersistence = 8,
  kMaxValue = kNotSyncingFailedMetadataPersistence,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.ApplySyncChangesState"
enum class ApplySyncChangesState {
  kApplyOK = 0,
  kApplyAddFailed = 1,
  kApplyUpdateFailed = 2,
  kApplyDeleteFailed = 3,
  kApplyMetadataChangesFailed = 4,

  kMaxValue = kApplyMetadataChangesFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordGeneration.SubmissionEvent"
enum PasswordSubmissionEvent {
  PASSWORD_SUBMITTED = 0,
  PASSWORD_SUBMISSION_FAILED = 1,
  PASSWORD_NOT_SUBMITTED = 2,
  PASSWORD_OVERRIDDEN = 3,
  PASSWORD_USED = 4,
  GENERATED_PASSWORD_FORCE_SAVED = 5,
  SUBMISSION_EVENT_ENUM_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum AutoSigninPromoUserAction {
  AUTO_SIGNIN_NO_ACTION = 0,
  AUTO_SIGNIN_TURN_OFF = 1,
  AUTO_SIGNIN_OK_GOT_IT = 2,
  AUTO_SIGNIN_PROMO_ACTION_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum AccountChooserUserAction {
  ACCOUNT_CHOOSER_DISMISSED = 0,
  ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN = 1,
  ACCOUNT_CHOOSER_SIGN_IN = 2,
  ACCOUNT_CHOOSER_ACTION_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.Mediation{Silent,Optional,Required}"
enum class CredentialManagerGetResult {
  // The promise is rejected.
  kRejected = 0,
  // Auto sign-in is not allowed in the current context.
  kNoneZeroClickOff = 1,
  // No matching credentials found.
  kNoneEmptyStore = 2,
  // User mediation required due to > 1 matching credentials.
  kNoneManyCredentials = 3,
  // User mediation required due to the signed out state.
  kNoneSignedOut = 4,
  // User mediation required due to pending first run experience dialog.
  kNoneFirstRun = 5,
  // Return empty credential for whatever reason.
  kNone = 6,
  // Return a credential from the account chooser.
  kAccountChooser = 7,
  // User is auto signed in.
  kAutoSignIn = 8,
  // No credentials are returned in incognito mode.
  kNoneIncognito = 9,
  // No credentials are returned while autofill_assistant is running. Deprecated
  // as part of autofill_assistant removal.
  // kNoneAutofillAssistant = 10,
  kMaxValue = kNoneIncognito,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum PasswordReusePasswordFieldDetected {
  NO_PASSWORD_FIELD = 0,
  HAS_PASSWORD_FIELD = 1,
  PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SubmittedFormFrame {
  MAIN_FRAME = 0,
  IFRAME_WITH_SAME_URL_AS_MAIN_FRAME = 1,
  IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME = 2,
  // Deprecated and replaced with a combination of buckets 4 & 5.
  // IFRAME_WITH_DIFFERENT_SIGNON_REALM = 3,
  IFRAME_WITH_PSL_MATCHED_SIGNON_REALM = 4,
  IFRAME_WITH_DIFFERENT_AND_NOT_PSL_MATCHED_SIGNON_REALM = 5,
  SUBMITTED_FORM_FRAME_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.AccessPasswordInSettings"
enum AccessPasswordInSettingsEvent {
  ACCESS_PASSWORD_VIEWED = 0,
  ACCESS_PASSWORD_COPIED = 1,
  ACCESS_PASSWORD_EDITED = 2,
  ACCESS_PASSWORD_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with
// "PasswordManager.ReauthResult" in enums.xml.
// Metrics: PasswordManager.ReauthToAccessPasswordInSettings
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class ReauthResult {
  kSuccess = 0,
  kFailure = 1,
  kSkipped = 2,
  kMaxValue = kSkipped,
};

// Specifies the type of PasswordFormManagers and derived classes to distinguish
// the context in which a PasswordFormManager is being created and used.
enum class CredentialSourceType {
  kUnknown = 0,
  // This is used for form based credential management (PasswordFormManager).
  kPasswordManager = 1,
  // This is used for credential management API based credential management
  // (CredentialManagerPasswordFormManager).
  kCredentialManagementAPI = 2
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: PasswordManager.DeleteCorruptedPasswordsResult
// Metrics: PasswordManager.DeleteUndecryptableLoginsReturnValue
// A passwords is considered corrupted if it's stored locally using lost
// encryption key.
enum class DeleteCorruptedPasswordsResult {
  // No corrupted entries were deleted.
  kSuccessNoDeletions = 0,
  // There were corrupted entries that were successfully deleted.
  kSuccessPasswordsDeleted = 1,
  // There was at least one corrupted entry that failed to be removed (it's
  // possible that other corrupted entries were deleted).
  kItemFailure = 2,
  // Encryption is unavailable, it's impossible to determine which entries are
  // corrupted.
  kEncryptionUnavailable = 3,
  kMaxValue = kEncryptionUnavailable,
};

enum class GaiaPasswordHashChange {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Password hash saved event where the account is used to sign in to Chrome
  // (syncing).
  SAVED_ON_CHROME_SIGNIN = 0,
  // Password hash saved in content area.
  SAVED_IN_CONTENT_AREA = 1,
  // Clear password hash when the account is signed out of Chrome.
  CLEARED_ON_CHROME_SIGNOUT = 2,
  // Password hash changed event where the account is used to sign in to Chrome
  // (syncing).
  CHANGED_IN_CONTENT_AREA = 3,
  // Password hash changed event where the account is not syncing.
  NOT_SYNC_PASSWORD_CHANGE = 4,
  // Password hash change event for non-GAIA enterprise accounts.
  NON_GAIA_ENTERPRISE_PASSWORD_CHANGE = 5,
  SAVED_SYNC_PASSWORD_CHANGE_COUNT = 6,
  kMaxValue = SAVED_SYNC_PASSWORD_CHANGE_COUNT,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IsSyncPasswordHashSaved {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  NOT_SAVED = 0,
  SAVED_VIA_STRING_PREF = 1,
  SAVED_VIA_LIST_PREF = 2,
  IS_SYNC_PASSWORD_HASH_SAVED_COUNT = 3,
  kMaxValue = IS_SYNC_PASSWORD_HASH_SAVED_COUNT,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metrics: "PasswordManager.CertificateErrorsWhileSeeingForms"
enum class CertificateError {
  NONE = 0,
  OTHER = 1,
  AUTHORITY_INVALID = 2,
  DATE_INVALID = 3,
  COMMON_NAME_INVALID = 4,
  WEAK_SIGNATURE_ALGORITHM = 5,
  COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metric: "PasswordManager.ReusedPasswordType".
enum class PasswordType {
  // Passwords saved by password manager.
  SAVED_PASSWORD = 0,
  // Passwords used for Chrome sign-in and is closest ("blessed") to be set to
  // sync when signed into multiple profiles if user wants to set up sync.
  // The primary account is equivalent to the "sync account" if this profile has
  // enabled sync.
  PRIMARY_ACCOUNT_PASSWORD = 1,
  // Other Gaia passwords used in Chrome other than the sync password.
  OTHER_GAIA_PASSWORD = 2,
  // Passwords captured from enterprise login page.
  ENTERPRISE_PASSWORD = 3,
  // Unknown password type. Used by downstream code to indicate there was not a
  // password reuse.
  PASSWORD_TYPE_UNKNOWN = 4,
  PASSWORD_TYPE_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LinuxBackendMigrationStatus {
  // No migration was attempted (this value should not occur).
  kNotAttempted = 0,
  // The last attempt was not completed.
  kDeprecatedFailed = 1,
  // All the data is in the encrypted loginDB.
  kDeprecatedCopiedAll = 2,
  // The standard login database is encrypted.
  kLoginDBReplaced = 3,
  // The migration is about to be attempted.
  kStarted = 4,
  // No access to the native backend.
  kPostponed = 5,
  // Could not create or write into the temporary file. Deprecated and replaced
  // by more precise errors.
  kDeprecatedFailedCreatedEncrypted = 6,
  // Could not read from the native backend.
  kDeprecatedFailedAccessNative = 7,
  // Could not replace old database.
  kFailedReplace = 8,
  // Could not initialise the temporary encrypted database.
  kFailedInitEncrypted = 9,
  // Could not reset th temporary encrypted database.
  kDeprecatedFailedRecreateEncrypted = 10,
  // Could not add entries into the temporary encrypted database.
  kFailedWriteToEncrypted = 11,
  kMaxValue = kFailedWriteToEncrypted
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Type of the password drop-down shown on focus field.
enum class PasswordDropdownState {
  // The passwords are listed and maybe the "Show all" button.
  kStandard = 0,
  // The drop down shows passwords and "Generate password" item.
  kStandardGenerate = 1,
  kMaxValue = kStandardGenerate
};

// Type of the item the user selects in the password drop-down.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordDropdownSelectedOption {
  // User selected a credential to fill.
  kPassword = 0,
  // User decided to open the password list.
  kShowAll = 1,
  // User selected to generate a password.
  kGenerate = 2,
  // User unlocked the account-store to fill a password.
  kUnlockAccountStorePasswords = 3,
  // User unlocked the account-store to generate a password.
  kUnlockAccountStoreGeneration = 4,
  // Previoulsy opted-in user decided to log-in again to access their passwords.
  kResigninToUnlockAccountStore = 5,
  // User selected a WebAuthn credential.
  kWebAuthn = 6,
  // User selected the "Sign in with another device" button.
  kWebAuthnSignInWithAnotherDevice = 7,
  kMaxValue = kWebAuthnSignInWithAnotherDevice
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Metric: "KeyboardAccessory.GenerationDialogChoice.{Automatic, Manual}".
enum class GenerationDialogChoice {
  // The user accepted the generated password.
  kAccepted = 0,
  // The user rejected the generated password.
  kRejected = 1,
  kMaxValue = kRejected
};

// Represents the state of the user wrt. sign-in and account-scoped storage.
// Used for metrics. Always keep this enum in sync with the corresponding
// histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUserState {
  // Signed-out user (and no account storage opt-in exists).
  kSignedOutUser = 0,
  // Signed-out user, but an account storage opt-in exists.
  kSignedOutAccountStoreUser = 1,
  // Signed-in non-syncing user, not opted in to the account storage (but may
  // save passwords to the account storage by default).
  kSignedInUser = 2,
  // Signed-in non-syncing user, not opted in to the account storage, and has
  // explicitly chosen to save passwords only on the device.
  kSignedInUserSavingLocally = 3,
  // Signed-in non-syncing user, opted in to the account storage, and saving
  // passwords to the account storage.
  kSignedInAccountStoreUser = 4,
  // Signed-in non-syncing user and opted in to the account storage, but has
  // chosen to save passwords only on the device.
  kSignedInAccountStoreUserSavingLocally = 5,
  // Syncing user.
  kSyncUser = 6,
};

// Represents different user interactions related to password check.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordCheckInteraction in enums.xml and
// password_manager_proxy.js.
enum class PasswordCheckInteraction {
  kAutomaticPasswordCheck = 0,
  kManualPasswordCheck = 1,
  kPasswordCheckStopped = 2,
  kChangePassword = 3,
  kEditPassword = 4,
  kRemovePassword = 5,
  kShowPassword = 6,
  kMutePassword = 7,
  kUnmutePassword = 8,
  kChangePasswordAutomatically = 9,
  // Must be last.
  kMaxValue = kChangePasswordAutomatically,
};

// Represents different user interactions related to adding credential from the
// setting. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused. Always keep this enum in sync with
// the corresponding PasswordCheckInteraction in enums.xml and
// password_manager_proxy.js.
enum class AddCredentialFromSettingsUserInteractions {
  // Used when the add credential dialog is opened from the settings.
  kAddDialogOpened = 0,
  // Used when the add credential dialog is closed from the settings.
  kAddDialogClosed = 1,
  // Used when a new credential is added from the settings .
  kCredentialAdded = 2,
  // Used when a new credential is being added from the add credential dialog in
  // settings and another credential exists with the same username/website
  // combination.
  kDuplicatedCredentialEntered = 3,
  // Used when an existing credential is viewed while adding a new credential
  // from the settings.
  kDuplicateCredentialViewed = 4,
  kMaxValue = kDuplicateCredentialViewed
};

// Metrics: PasswordManager.MoveToAccountStoreTrigger.
// This must be kept in sync with the enum in password_move_to_account_dialog.js
// (in chrome/browser/resources/settings/autofill_page).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MoveToAccountStoreTrigger {
  // The user successfully logged in with a password from the profile store.
  kSuccessfulLoginWithProfileStorePassword = 0,
  // The user explicitly asked to move a password listed in Settings.
  kExplicitlyTriggeredInSettings = 1,
  // The user explicitly asked to move multiple passwords at once in Settings.
  kExplicitlyTriggeredForMultiplePasswordsInSettings = 2,
  // After saving a password locally, the user opted in to saving this and
  // future passwords in the account.
  kUserOptedInAfterSavingLocally = 3,
  kMaxValue = kUserOptedInAfterSavingLocally,
};

// Used to record metrics for the usage and timing of the GetChangePasswordUrl
// call. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class GetChangePasswordUrlMetric {
  // Used when GetChangePasswordUrl is called before the response
  // arrives.
  kNotFetchedYet = 0,
  // Used when a url was used, which corresponds to the requested site.
  kUrlOverrideUsed = 1,
  // Used when no override url was available.
  kNoUrlOverrideAvailable = 2,
  // Used when a url was used, which corresponds to a site from within same
  // FacetGroup.
  kGroupUrlOverrideUsed = 3,
  kMaxValue = kGroupUrlOverrideUsed,
};

// Used to record what exactly was updated during password editing flow.
// Entries should not be renumbered and numeric values should never be reused.
enum class PasswordEditUpdatedValues {
  // Nothing was updated.
  kNone = 0,
  // Only username was changed.
  kUsername = 1,
  // Only password was changed.
  kPassword = 2,
  // Both password and username were updated.
  kBoth = 3,
  kMaxValue = kBoth,
};

// Used to record usage of the note field in password editing / adding flows in
// the settings UI. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class PasswordNoteAction {
  // A new credential is added from settings, with the note field not empty.
  kNoteAddedInAddDialog = 0,
  // Note changed from empty to non-empty from the password edit dialog in
  // settings.
  kNoteAddedInEditDialog = 1,
  // Note changed from non-empty to another non-empty from the password edit
  // dialog in settings.
  kNoteEditedInEditDialog = 2,
  // Note changed from non-empty to empty from the password edit dialog in
  // settings.
  kNoteRemovedInEditDialog = 3,
  // Note did not change.
  kNoteNotChanged = 4,
  kMaxValue = kNoteNotChanged,
};

std::string GetPasswordAccountStorageUserStateHistogramSuffix(
    PasswordAccountStorageUserState user_state);

// The usage level of the account-scoped password storage. This is essentially
// a less-detailed version of PasswordAccountStorageUserState, for metrics that
// don't need the fully-detailed breakdown.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordAccountStorageUsageLevel {
  // The user is not using the account-scoped password storage. Either they're
  // not signed in, or they haven't opted in to the account storage.
  kNotUsingAccountStorage = 0,
  // The user is signed in (but not syncing) and has opted in to the account
  // storage.
  kUsingAccountStorage = 1,
  // The user has enabled Sync.
  kSyncing = 2,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordViewPageInteractions {
  // The credential row is clicked in the password list in settings.
  kCredentialRowClicked = 0,
  // The user opens the password view page to view an existing credential.
  kCredentialFound = 1,
  // The user opens the password view page to view an non-existing credential.
  // This will close the settings password view page.
  kCredentialNotFound = 2,
  // The copy username button in settings password view page is clicked.
  kUsernameCopyButtonClicked = 3,
  // The copy password button in settings password view page is clicked.
  kPasswordCopyButtonClicked = 4,
  // The show password button in settings password view page is clicked and the
  // password is revealed.
  kPasswordShowButtonClicked = 5,
  // The edit button in settings password view page is clicked.
  kPasswordEditButtonClicked = 6,
  // The delete button in settings password view page is clicked.
  kPasswordDeleteButtonClicked = 7,
  // The credential's username, password or note is edited in settings password
  // view page.
  kCredentialEdited = 8,
  // The password view page is closed while the edit dialog is open after
  // an authentication timeout.
  kTimedOutInEditDialog = 9,
  // The password view page is closed while the edit dialog is closed after
  // an authentication timeout.
  kTimedOutInViewPage = 10,
  // The credential is requested by typing the URL.
  kCredentialRequestedByUrl = 11,
  kMaxValue = kCredentialRequestedByUrl,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordsImportDesktopInteractions {
  // Import dialog opened from the three-dot menu.
  kDialogOpenedFromThreeDot = 0,
  // Import dialog opened from the empty passwords list.
  kDialogOpenedFromEmptyState = 1,
  // Import flow canceled before the file selection.
  kCanceledBeforeFileSelect = 2,
  kMaxValue = kCanceledBeforeFileSelect,
};

// Represents different user interactions related to managing credentials from
// the password management bubble opened from the key icon in omnibox.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordManagementBubbleInteractions in enums.xml.
enum class PasswordManagementBubbleInteractions {
  kManagePasswordsButtonClicked = 0,
  kGooglePasswordManagerLinkClicked = 1,
  kCredentialRowWithoutNoteClicked = 2,
  kUsernameCopyButtonClicked = 3,
  kPasswordCopyButtonClicked = 4,
  kPasswordShowButtonClicked = 5,
  kUsernameEditButtonClicked = 6,
  kUsernameAdded = 7,
  kNoteEditButtonClicked = 8,
  kNoteAdded = 9,
  kNoteEdited = 10,
  kNoteDeleted = 11,
  kCredentialRowWithNoteClicked = 12,
  kMaxValue = kCredentialRowWithNoteClicked,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordManagerShortcutMetric {
  // User clicked "Add shortcut" from the UI.
  kAddShortcutClicked = 0,
  // Shortcut was successfully installed .
  kShortcutInstalled = 1,
  // User switched profile in the standalone password manager app.
  kProfileSwitched = 2,
  kMaxValue = kProfileSwitched,
};

std::string GetPasswordAccountStorageUsageLevelHistogramSuffix(
    PasswordAccountStorageUsageLevel usage_level);

// Records the `type` of a leak dialog shown to the user and the `reason`
// why it was dismissed.
class LeakDialogMetricsRecorder {
 public:
  // Create a LeakDialogMetricsRecorder corresponding to a navigation with
  // `source_id`.
  LeakDialogMetricsRecorder(ukm::SourceId source_id, LeakDialogType type);
  LeakDialogMetricsRecorder(const LeakDialogMetricsRecorder&) = delete;
  LeakDialogMetricsRecorder& operator=(const LeakDialogMetricsRecorder&) =
      delete;

  // Log the `reason` for dismissing the leak warning dialog, e.g. signaling
  // that the user ignored it or that they asked for an automatic password
  // change.
  void LogLeakDialogTypeAndDismissalReason(LeakDialogDismissalReason reason);

  // Helper method to overwrite the sampling rate during unit tests.
  void SetSamplingRateForTesting(double rate) { ukm_sampling_rate_ = rate; }

 private:
  // The UMA prefix.
  static constexpr char kHistogram[] =
      "PasswordManager.LeakDetection.DialogDismissalReason";

  // The sampling rate for UKM recording. A value of 0.1 corresponds to a
  // sampling rate of 10%.
  double ukm_sampling_rate_ = 0.1;

  // Helper method to determine the suffix for the UMA.
  const char* GetUMASuffix() const;

  // The source id associated with the navigation.
  ukm::SourceId source_id_;

  // The type of the leak dialog.
  LeakDialogType type_;
};

// Log the |reason| a user dismissed the password manager UI except save/update
// bubbles.
void LogGeneralUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the save password bubble. If
// |user_state| is set, the |reason| is also logged to a separate
// user-state-specific histogram. If the submission is detected on a cleared
// change password form, dismissal reason is also recorded in a histogram
// specific for this type of submission.
void LogSaveUIDismissalReason(
    UIDismissalReason reason,
    autofill::mojom::SubmissionIndicatorEvent submission_event,
    absl::optional<PasswordAccountStorageUserState> user_state);

// Log the |reason| a user dismissed the save password prompt after previously
// having unblocklisted the origin while on the page.
void LogSaveUIDismissalReasonAfterUnblocklisting(UIDismissalReason reason);

// Log the |reason| a user dismissed the update password bubble. If the
// submission is detected on a cleared change password form, dismissal reason is
// also recorded in a histogram specific for this type of submission.
void LogUpdateUIDismissalReason(
    UIDismissalReason reason,
    autofill::mojom::SubmissionIndicatorEvent submission_event);

// Log the |reason| a user dismissed the move password bubble.
void LogMoveUIDismissalReason(UIDismissalReason reason,
                              PasswordAccountStorageUserState user_state);

// Log the appropriate display disposition.
void LogUIDisplayDisposition(UIDisplayDisposition disposition);

// Log if a saved FormData was deserialized correctly.
void LogFormDataDeserializationStatus(FormDeserializationStatus status);

// When a credential was filled, log whether it came from an Android app.
void LogFilledCredentialIsFromAndroidApp(bool from_android);

// Log what's preventing passwords from syncing.
void LogPasswordSyncState(PasswordSyncState state);

// Log what's preventing passwords from applying sync changes.
void LogApplySyncChangesState(ApplySyncChangesState state);

// Log submission events related to generation.
void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event);

// Log when password generation is available for a particular form.
void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event);

// Log a user action on showing the autosignin first run experience.
void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action);

// Log a user action on showing the account chooser for one or many accounts.
void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action);
void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action);

// Log the result of navigator.credentials.get.
void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation);

// Log the password reuse.
void LogPasswordReuse(int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type);

// Log the type of the password dropdown when it's shown.
void LogPasswordDropdownShown(PasswordDropdownState state, bool off_the_record);

// Log the type of the password dropdown suggestion when chosen.
void LogPasswordDropdownItemSelected(PasswordDropdownSelectedOption type,
                                     bool off_the_record);

// Log a password successful submission event.
void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Log a password successful submission event for accepted by user password save
// or update.
void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Logs how many account-stored passwords are available for filling in the
// current password form right after unlock.
void LogPasswordsCountFromAccountStoreAfterUnlock(
    int account_store_passwords_count);

// Logs how many account-stored passwords are downloaded right after unlock.
// This is different from `LogPasswordsCountFromAccountStoreAfterUnlock` since
// it records all the downloaded passwords not just those available for filling
// in a specific password form.
void LogDownloadedPasswordsCountFromAccountStoreAfterUnlock(
    int account_store_passwords_count);

// Logs how many blocklisted entries are downloaded to the account store right
// after unlock.
void LogDownloadedBlocklistedEntriesCountFromAccountStoreAfterUnlock(
    int blocklist_entries_count);

// Logs the result of a re-auth challenge in the password settings.
void LogPasswordSettingsReauthResult(ReauthResult result);

// Log a return value of LoginDatabase::DeleteUndecryptableLogins method.
void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result);

// Log metrics about a newly saved password (e.g. whether a saved password was
// generated).
void LogNewlySavedPasswordMetrics(
    bool is_generated_password,
    bool is_username_empty,
    PasswordAccountStorageUsageLevel account_storage_usage_level);

// Log whether the generated password was accepted or rejected for generation of
// |type| (automatic or manual).
void LogGenerationDialogChoice(
    GenerationDialogChoice choice,
    autofill::password_generation::PasswordGenerationType type);

// Log a save gaia password change event.
void LogGaiaPasswordHashChange(GaiaPasswordHashChange event,
                               bool is_sync_password);

// Log whether a sync password hash saved.
void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection);

// Log whether the saved password is protected by Phishguard. To preserve
// privacy of individual data points, we will log with 10% noise.
void LogIsPasswordProtected(bool is_password_protected);

// Log the number of Gaia password hashes saved. Currently only called on
// profile start up.
void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    bool does_primary_account_exists,
                                    bool is_signed_in);

// Log the user interaction events when creating a new credential from settings.
void LogUserInteractionsWhenAddingCredentialFromSettings(
    AddCredentialFromSettingsUserInteractions
        add_credential_from_settings_user_interaction);

// Log the user interaction with the note field in password add / edit dialogs.
void LogPasswordNoteActionInSettings(PasswordNoteAction action);

// Log the user interaction events with a revamped password management bubble
// opened from the key icon in omnibox.
void LogUserInteractionsInPasswordManagementBubble(
    PasswordManagementBubbleInteractions
        password_management_bubble_interaction);

// Wraps |callback| into another callback that measures the elapsed time between
// construction and actual execution of the callback. Records the result to
// |histogram|, which is expected to be a char literal.
template <typename R, typename... Args>
base::OnceCallback<R(Args...)> TimeCallback(
    base::OnceCallback<R(Args...)> callback,
    const char* histogram) {
  return base::BindOnce(
      [](const char* histogram, const base::ElapsedTimer& timer,
         base::OnceCallback<R(Args...)> callback, Args... args) {
        base::UmaHistogramTimes(histogram, timer.Elapsed());
        return std::move(callback).Run(std::forward<Args>(args)...);
      },
      histogram, base::ElapsedTimer(), std::move(callback));
}

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
