// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Settings} from '../common/settings.js';

import {AutoScanManager} from './auto_scan_manager.js';

const PrefType = chrome.settingsPrivate.PrefType;

/**
 * Class to manage user preferences.
 */
export class SettingsManager {
  static async init() {
    await Settings.init(Object.values(Preference));
    Settings.addListener(
        Preference.AUTO_SCAN_ENABLED,
        pref => AutoScanManager.setEnabled(Boolean(pref.value)));
    Settings.addListener(
        Preference.AUTO_SCAN_TIME,
        pref => AutoScanManager.setPrimaryScanTime(
            /** @type {number} */ (pref.value)));
    Settings.addListener(
        Preference.AUTO_SCAN_KEYBOARD_TIME,
        pref => AutoScanManager.setKeyboardScanTime(
            /** @type {number} */ (pref.value)));

    if (!SettingsManager.settingsAreConfigured_()) {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/switchAccess');
    }
  }

  // =============== Private Methods ==============

  /**
   * Whether the current settings configuration is reasonably usable;
   * specifically, whether there is a way to select and a way to navigate.
   * @return {boolean}
   * @private
   */
  static settingsAreConfigured_() {
    const selectPref = Settings.get(Preference.SELECT_DEVICE_KEY_CODES);
    const selectSet = selectPref ? Object.keys(selectPref).length : false;

    const nextPref = Settings.get(Preference.NEXT_DEVICE_KEY_CODES);
    const nextSet = nextPref ? Object.keys(nextPref).length : false;

    const previousPref = Settings.get(Preference.PREVIOUS_DEVICE_KEY_CODES);
    const previousSet = previousPref ? Object.keys(previousPref).length : false;

    const autoScanEnabled = Settings.get(Preference.AUTO_SCAN_ENABLED);

    if (!selectSet) {
      return false;
    }

    if (nextSet || previousSet) {
      return true;
    }

    return Boolean(autoScanEnabled);
  }
}

/**
 * Preferences that are configurable in Switch Access.
 * @enum {string}
 */
const Preference = {
  AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
  AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
  AUTO_SCAN_KEYBOARD_TIME:
      'settings.a11y.switch_access.auto_scan.keyboard.speed_ms',
  NEXT_DEVICE_KEY_CODES: 'settings.a11y.switch_access.next.device_key_codes',
  PREVIOUS_DEVICE_KEY_CODES:
      'settings.a11y.switch_access.previous.device_key_codes',
  SELECT_DEVICE_KEY_CODES:
      'settings.a11y.switch_access.select.device_key_codes',
};
