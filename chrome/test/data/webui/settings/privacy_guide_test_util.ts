// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {CookiePrimarySetting, PrivacyGuideStep, SafeBrowsingSetting, SettingsPrivacyGuidePageElement} from 'chrome://settings/lazy_load.js';
import {Router, routes, SettingsPrefsElement, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {getSyncAllPrefs} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

export function setupPrivacyRouteForTest(): void {
  // Simulates the route of the user entering the privacy guide from the S&P
  // settings. This is necessary as tests seem to by default define the
  // previous route as Settings "/". On a back navigation, "/" matches the
  // criteria for a valid Settings parent no matter how deep the subpage is in
  // the Settings tree. This would always navigate to Settings "/" instead of
  // to the parent of the current subpage.
  Router.getInstance().navigateTo(routes.PRIVACY);
}

/**
 * Equivalent of the user manually navigating to the corresponding step via
 * typing the URL and step parameter in the Omnibox.
 */
export function navigateToStep(step: PrivacyGuideStep): Promise<void> {
  Router.getInstance().navigateTo(
      routes.PRIVACY_GUIDE,
      /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
  return flushTasks();
}

// Set all relevant sync status and fire a changed event and flush the UI.
export function setupSync({
  syncBrowserProxy,
  syncOn,
  syncAllDataTypes,
  typedUrlsSynced,
}: {
  syncBrowserProxy: TestSyncBrowserProxy,
  syncAllDataTypes: boolean,
  typedUrlsSynced: boolean,
  syncOn: boolean,
}): void {
  if (syncAllDataTypes) {
    assertTrue(typedUrlsSynced);
  }
  if (typedUrlsSynced) {
    assertTrue(syncOn);
  }
  syncBrowserProxy.testSyncStatus = {
    signedIn: syncOn,
    hasError: false,
    statusAction: StatusAction.NO_ACTION,
  };
  webUIListenerCallback('sync-status-changed', syncBrowserProxy.testSyncStatus);

  const event = getSyncAllPrefs();
  // Overwrite datatypes needed in tests.
  event.syncAllDataTypes = syncAllDataTypes;
  event.typedUrlsSynced = typedUrlsSynced;
  webUIListenerCallback('sync-prefs-changed', event);
}

// Set the cookies setting for the privacy guide.
export function setCookieSetting(
    page: SettingsPrivacyGuidePageElement,
    setting: CookiePrimarySetting): void {
  page.set('prefs.generated.cookie_primary_setting', {
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: setting,
  });
}

export function shouldShowCookiesCard(page: SettingsPrivacyGuidePageElement):
    boolean {
  const setting = page.getPref('generated.cookie_primary_setting').value;
  return setting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
      setting === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
}

// Set the safe browsing setting for the privacy guide.
export function setSafeBrowsingSetting(
    page: SettingsPrivacyGuidePageElement, setting: SafeBrowsingSetting): void {
  page.set('prefs.generated.safe_browsing', {
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: setting,
  });
}

export function shouldShowSafeBrowsingCard(
    page: SettingsPrivacyGuidePageElement): boolean {
  const setting = page.getPref('generated.safe_browsing').value;
  return setting === SafeBrowsingSetting.ENHANCED ||
      setting === SafeBrowsingSetting.STANDARD;
}

export function shouldShowHistorySyncCard(
    syncBrowserProxy: TestSyncBrowserProxy): boolean {
  return !syncBrowserProxy.testSyncStatus ||
      !!syncBrowserProxy.testSyncStatus.signedIn;
}

// Bundles functionality to create the page object for tests.
export function createPrivacyGuidePageForTest(
    settingsPrefs: SettingsPrefsElement): SettingsPrivacyGuidePageElement {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const page = document.createElement('settings-privacy-guide-page');
  page.disableAnimationsForTesting();
  page.prefs = settingsPrefs.prefs!;
  document.body.appendChild(page);

  setupPrivacyRouteForTest();

  return page;
}

// Bundles frequently used functionality to configure the page object for tests.
export function setupPrivacyGuidePageForTest(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy): void {
  setSafeBrowsingSetting(page, SafeBrowsingSetting.STANDARD);
  setCookieSetting(page, CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
  setupSync({
    syncBrowserProxy: syncBrowserProxy,
    syncOn: true,
    syncAllDataTypes: true,
    typedUrlsSynced: true,
  });
}

export function setParametersForHistorySyncStep(
    syncBrowserProxy: TestSyncBrowserProxy, isEligible: boolean): void {
  if (!isEligible) {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
  }
  assertEquals(
      isEligible, shouldShowHistorySyncCard(syncBrowserProxy),
      'Parameters for HistorySync are set incorrectly.');
}

export function setParametersForSafeBrowsingStep(
    page: SettingsPrivacyGuidePageElement, isEligible: boolean): void {
  page.setPrefValue(
      'generated.safe_browsing',
      isEligible ? SafeBrowsingSetting.STANDARD : SafeBrowsingSetting.DISABLED);
  assertEquals(
      isEligible, shouldShowSafeBrowsingCard(page),
      'Parameters for SafeBrowsing are set incorrectly.');
}

export function setParametersForCookiesStep(
    page: SettingsPrivacyGuidePageElement, isEligible: boolean): void {
  page.setPrefValue(
      'generated.cookie_primary_setting',
      isEligible ? CookiePrimarySetting.BLOCK_THIRD_PARTY :
                   CookiePrimarySetting.ALLOW_ALL);
  assertEquals(
      isEligible, shouldShowCookiesCard(page),
      'Parameters for Cookies are set incorrectly.');
}

export function clickNextOnWelcomeStep(page: SettingsPrivacyGuidePageElement):
    Promise<void> {
  const welcomeFragment = page.shadowRoot!.querySelector<HTMLElement>(
      '#' + PrivacyGuideStep.WELCOME);
  assertTrue(!!welcomeFragment, 'Welcome fragment is null.');
  assertTrue(isChildVisible(page, '#' + PrivacyGuideStep.WELCOME));
  welcomeFragment.dispatchEvent(
      new CustomEvent('start-button-click', {bubbles: true, composed: true}));
  return flushTasks();
}