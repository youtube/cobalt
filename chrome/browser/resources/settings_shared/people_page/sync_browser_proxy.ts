// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
export interface StoredAccount {
  fullName?: string;
  givenName?: string;
  email: string;
  avatarImage?: string;
}

/**
 * TODO(crbug.com/1322559): signedIn doesn't indicate if the user is signed-in,
 * but instead if the user is syncing.
 * TODO(crbug.com/1107771): childUser and supervisedUser are only consumed
 * together and the latter implies the former, so it should be enough to have
 * only one of them here. The linked bug has other clean-up suggestions.
 * TODO(crbug.com/1107771): signedIn actually means having primary account with
 * sync consent. Rename to make this clear.
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
export interface SyncStatus {
  statusAction: StatusAction;
  childUser?: boolean;
  disabled?: boolean;
  domain?: string;
  hasError?: boolean;
  hasPasswordsOnlyError?: boolean;
  hasUnrecoverableError?: boolean;
  managed?: boolean;
  firstSetupInProgress?: boolean;
  signedIn?: boolean;
  signedInUsername?: string;
  statusActionText?: string;
  statusText?: string;
  supervisedUser?: boolean;
  syncSystemEnabled?: boolean;
}

/**
 * Must be kept in sync with the return values of getSyncErrorAction in
 * chrome/browser/ui/webui/settings/people_handler.cc
 */
export enum StatusAction {
  NO_ACTION = 'noAction',                // No action to take.
  REAUTHENTICATE = 'reauthenticate',     // User needs to reauthenticate.
  UPGRADE_CLIENT = 'upgradeClient',      // User needs to upgrade the client.
  ENTER_PASSPHRASE = 'enterPassphrase',  // User needs to enter passphrase.
  // User needs to go through key retrieval.
  RETRIEVE_TRUSTED_VAULT_KEYS = 'retrieveTrustedVaultKeys',
  CONFIRM_SYNC_SETTINGS =
      'confirmSyncSettings',  // User needs to confirm sync settings.
}

/**
 * The state of sync. This is the data structure sent back and forth between
 * C++ and JS. Its naming and structure is not optimal, but changing it would
 * require changes to the C++ handler, which is already functional. See
 * PeopleHandler::PushSyncPrefs() for more details.
 */
export interface SyncPrefs {
  appsRegistered: boolean;
  appsSynced: boolean;
  autofillRegistered: boolean;
  autofillSynced: boolean;
  bookmarksRegistered: boolean;
  bookmarksSynced: boolean;
  customPassphraseAllowed: boolean;
  encryptAllData: boolean;
  extensionsRegistered: boolean;
  extensionsSynced: boolean;
  passphraseRequired: boolean;
  passwordsRegistered: boolean;
  passwordsSynced: boolean;
  paymentsIntegrationEnabled: boolean;
  preferencesRegistered: boolean;
  preferencesSynced: boolean;
  readingListRegistered: boolean;
  readingListSynced: boolean;
  syncAllDataTypes: boolean;
  tabsRegistered: boolean;
  tabsSynced: boolean;
  themesRegistered: boolean;
  themesSynced: boolean;
  trustedVaultKeysRequired: boolean;
  typedUrlsRegistered: boolean;
  typedUrlsSynced: boolean;
  wifiConfigurationsRegistered: boolean;
  wifiConfigurationsSynced: boolean;
  explicitPassphraseTime?: string;
}

/**
 * Names of the individual data type properties to be cached from
 * SyncPrefs when the user checks 'Sync All'.
 */
export const syncPrefsIndividualDataTypes: string[] = [
  'appsSynced',
  'autofillSynced',
  'bookmarksSynced',
  'extensionsSynced',
  'readingListSynced',
  'passwordsSynced',
  'paymentsIntegrationEnabled',
  'preferencesSynced',
  'savedTabGroupsSynced',
  'tabsSynced',
  'themesSynced',
  'typedUrlsSynced',
  'wifiConfigurationsSynced',
];

export enum PageStatus {
  SPINNER = 'spinner',      // Before the page has loaded.
  CONFIGURE = 'configure',  // Preferences ready to be configured.
  DONE = 'done',            // Sync subpage can be closed now.
  PASSPHRASE_FAILED = 'passphraseFailed',  // Error in the passphrase.
}

// WARNING: Keep synced with chrome/browser/ui/webui/settings/people_handler.cc.
export enum TrustedVaultBannerState {
  NOT_SHOWN = 0,
  OFFER_OPT_IN = 1,
  OPTED_IN = 2,
}

/**
 * Key to be used with localStorage.
 */
const PROMO_IMPRESSION_COUNT_KEY: string = 'signin-promo-count';

export interface SyncBrowserProxy {
  // <if expr="not chromeos_ash">
  /**
   * Starts the signin process for the user. Does nothing if the user is
   * already signed in.
   */
  startSignIn(): void;

  /**
   * Signs out the signed-in user.
   */
  signOut(deleteProfile: boolean): void;

  /**
   * Invalidates the Sync token without signing the user out.
   */
  pauseSync(): void;
  // </if>

  /**
   * @return the number of times the sync account promo was shown.
   */
  getPromoImpressionCount(): number;

  /**
   * Increment the number of times the sync account promo was shown.
   */
  incrementPromoImpressionCount(): void;

  // <if expr="chromeos_ash">
  /**
   * Signs the user out.
   */
  attemptUserExit(): void;

  /**
   * Turns on sync for the currently logged in user. Chrome OS users are
   * always signed in to Chrome.
   */
  turnOnSync(): void;

  /**
   * Turns off sync. Does not sign out of Chrome.
   */
  turnOffSync(): void;
  // </if>

  /**
   * Starts the key retrieval process.
   */
  startKeyRetrieval(): void;

  /**
   * Gets the current sync status.
   */
  getSyncStatus(): Promise<SyncStatus>;

  /**
   * Gets a list of stored accounts.
   */
  getStoredAccounts(): Promise<StoredAccount[]>;

  /**
   * Function to invoke when the sync page has been navigated to. This
   * registers the UI as the "active" sync UI so that if the user tries to
   * open another sync UI, this one will be shown instead.
   */
  didNavigateToSyncPage(): void;

  /**
   * Function to invoke when leaving the sync page so that the C++ layer can
   * be notified that the sync UI is no longer open.
   */
  didNavigateAwayFromSyncPage(didAbort: boolean): void;

  /**
   * Sets which types of data to sync.
   */
  setSyncDatatypes(syncPrefs: SyncPrefs): Promise<PageStatus>;

  /**
   * Attempts to set up a new passphrase to encrypt Sync data.
   * @return Whether the passphrase was successfully set. The call can fail, for
   *     example, if encrypting the data is disallowed.
   */
  setEncryptionPassphrase(passphrase: string): Promise<boolean>;

  /**
   * Attempts to set the passphrase to decrypt Sync data.
   * @return Whether the passphrase was successfully set. The call can fail, for
   *     example, if the passphrase is incorrect.
   */
  setDecryptionPassphrase(passphrase: string): Promise<boolean>;

  /**
   * Start syncing with an account, specified by its email.
   * |isDefaultPromoAccount| is true if |email| is the email of the default
   * account displayed in the promo.
   */
  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean): void;

  /**
   * Opens the Google Activity Controls url in a new tab.
   */
  openActivityControlsUrl(): void;

  /**
   * Function to dispatch event sync-prefs-changed even without a change.
   * This is used to decide whether we should show the link to password
   * manager in passwords section on page load.
   */
  sendSyncPrefsChanged(): void;

  /**
   * Forces a trusted-vault-banner-state-changed event to be fired.
   */
  sendTrustedVaultBannerStateChanged(): void;
}

export class SyncBrowserProxyImpl implements SyncBrowserProxy {
  // <if expr="not chromeos_ash">
  startSignIn() {
    chrome.send('SyncSetupStartSignIn');
  }

  signOut(deleteProfile: boolean) {
    chrome.send('SyncSetupSignout', [deleteProfile]);
  }

  pauseSync() {
    chrome.send('SyncSetupPauseSync');
  }
  // </if>

  getPromoImpressionCount() {
    return parseInt(
               window.localStorage.getItem(PROMO_IMPRESSION_COUNT_KEY)!, 10) ||
        0;
  }

  incrementPromoImpressionCount() {
    window.localStorage.setItem(
        PROMO_IMPRESSION_COUNT_KEY,
        (this.getPromoImpressionCount() + 1).toString());
  }

  // <if expr="chromeos_ash">
  attemptUserExit() {
    return chrome.send('AttemptUserExit');
  }

  turnOnSync() {
    return chrome.send('TurnOnSync');
  }

  turnOffSync() {
    return chrome.send('TurnOffSync');
  }
  // </if>

  startKeyRetrieval() {
    chrome.send('SyncStartKeyRetrieval');
  }

  getSyncStatus() {
    return sendWithPromise('SyncSetupGetSyncStatus');
  }

  getStoredAccounts() {
    return sendWithPromise('SyncSetupGetStoredAccounts');
  }

  didNavigateToSyncPage() {
    chrome.send('SyncSetupShowSetupUI');
  }

  didNavigateAwayFromSyncPage(didAbort: boolean) {
    chrome.send('SyncSetupDidClosePage', [didAbort]);
  }

  setSyncDatatypes(syncPrefs: SyncPrefs) {
    return sendWithPromise('SyncSetupSetDatatypes', JSON.stringify(syncPrefs));
  }

  setEncryptionPassphrase(passphrase: string) {
    return sendWithPromise('SyncSetupSetEncryptionPassphrase', passphrase);
  }

  setDecryptionPassphrase(passphrase: string) {
    return sendWithPromise('SyncSetupSetDecryptionPassphrase', passphrase);
  }

  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean) {
    chrome.send(
        'SyncSetupStartSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  openActivityControlsUrl() {
    chrome.metricsPrivate.recordUserAction(
        'Signin_AccountSettings_GoogleActivityControlsClicked');
  }

  sendSyncPrefsChanged() {
    chrome.send('SyncPrefsDispatch');
  }

  sendTrustedVaultBannerStateChanged() {
    chrome.send('SyncTrustedVaultBannerStateDispatch');
  }

  static getInstance(): SyncBrowserProxy {
    return instance || (instance = new SyncBrowserProxyImpl());
  }

  static setInstance(obj: SyncBrowserProxy) {
    instance = obj;
  }
}

let instance: SyncBrowserProxy|null = null;
