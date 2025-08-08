// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Route, Router, SettingsRoutes, StoredAccount, SyncPrefs, SyncStatus} from 'chrome://settings/settings.js';
// clang-format on

/**
 * Returns sync prefs with everything synced and no passphrase required.
 */
export function getSyncAllPrefs(): SyncPrefs {
  return {
    appsManaged: false,
    appsRegistered: true,
    appsSynced: true,
    autofillManaged: false,
    autofillRegistered: true,
    autofillSynced: true,
    bookmarksManaged: false,
    bookmarksRegistered: true,
    bookmarksSynced: true,
    encryptAllData: false,
    customPassphraseAllowed: true,
    extensionsManaged: false,
    extensionsRegistered: true,
    extensionsSynced: true,
    passphraseRequired: false,
    passwordsManaged: false,
    passwordsRegistered: true,
    passwordsSynced: true,
    paymentsManaged: false,
    paymentsRegistered: true,
    paymentsSynced: true,
    preferencesManaged: false,
    preferencesRegistered: true,
    preferencesSynced: true,
    readingListManaged: false,
    readingListRegistered: true,
    readingListSynced: true,
    savedTabGroupsManaged: false,
    savedTabGroupsRegistered: true,
    savedTabGroupsSynced: true,
    syncAllDataTypes: true,
    tabsManaged: false,
    tabsRegistered: true,
    tabsSynced: true,
    trustedVaultKeysRequired: false,
    themesManaged: false,
    themesRegistered: true,
    themesSynced: true,
    typedUrlsManaged: false,
    typedUrlsRegistered: true,
    typedUrlsSynced: true,
    wifiConfigurationsManaged: false,
    wifiConfigurationsRegistered: true,
    wifiConfigurationsSynced: true,
  };
}

export function getSyncAllPrefsManaged(): SyncPrefs {
  // Make all sync types in the SyncPrefs object disabled by policy.
  return Object.assign(getSyncAllPrefs(), {
    appsManaged: true,
    appsSynced: false,
    autofillManaged: true,
    autofillSynced: false,
    bookmarksManaged: true,
    bookmarksSynced: false,
    extensionsManaged: true,
    extensionsSynced: false,
    passwordsManaged: true,
    passwordsSynced: false,
    paymentsManaged: true,
    paymentsSynced: false,
    preferencesManaged: true,
    preferencesSynced: false,
    readingListManaged: true,
    readingListSynced: false,
    savedTabGroupsManaged: true,
    savedTabGroupsSynced: false,
    tabsManaged: true,
    tabsSynced: false,
    themesManaged: true,
    themesSynced: false,
    typedUrlsManaged: true,
    typedUrlsSynced: false,
    wifiConfigurationsManaged: true,
    wifiConfigurationsSynced: false,
  });
}

export interface SyncRoutes {
  BASIC: Route;
  PEOPLE: Route;
  SYNC: Route;
  SYNC_ADVANCED: Route;
  SIGN_OUT: Route;
  ADVANCED: Route;
  ABOUT: Route;
}

export function setupRouterWithSyncRoutes() {
  const BASIC = new Route('/');
  const PEOPLE = BASIC.createSection('/people', 'people');
  const SYNC = PEOPLE.createChild('/syncSetup');
  const SYNC_ADVANCED = SYNC.createChild('/syncSetup/advanced');

  const SIGN_OUT = BASIC.createChild('/signOut');
  SIGN_OUT.isNavigableDialog = true;

  const routes: SyncRoutes = {
    BASIC,
    PEOPLE,
    SYNC,
    SYNC_ADVANCED,
    SIGN_OUT,
    ADVANCED: new Route('/advanced'),
    ABOUT: new Route('/help'),
  };

  Router.resetInstanceForTesting(
      new Router(routes as unknown as SettingsRoutes));
}

export function simulateSyncStatus(status: SyncStatus|undefined) {
  webUIListenerCallback('sync-status-changed', status);
  flush();
}

export function simulateStoredAccounts(accounts: StoredAccount[]|undefined) {
  webUIListenerCallback('stored-accounts-updated', accounts);
  flush();
}
