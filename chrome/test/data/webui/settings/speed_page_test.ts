// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkPredictionOptions} from 'chrome://settings/lazy_load.js';
import type {SettingsDropdownMenuElement, SettingsPrefsElement, SpeedPageElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PerformanceBrowserProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind, flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('SpeedPage', function() {
  let speedPage: SpeedPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function getFakePrefs() {
    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        // By default the pref is initialized to WIFI_ONLY_DEPRECATED, but then
        // treated as STANDARD. See chrome/browser/preloading/preloading_prefs.h
        // for more details.
        value: NetworkPredictionOptions.WIFI_ONLY_DEPRECATED,
      },
      {
        key: 'cpu_performance_tier_override',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: -1,
      },
    ];
    return fakePrefs;
  }

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async () => {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    CrSettingsPrefs.resetForTesting();
    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    // Wait until settings are initialized to start tests.
    await CrSettingsPrefs.initialized;

    speedPage = document.createElement('settings-speed-page');
    speedPage.prefs = settingsPrefs.prefs!;
    fakeDataBind(settingsPrefs, speedPage, 'prefs');
    document.body.appendChild(speedPage);
    await microtasksFinished();
  });

  test('PreloadPagesDefault', function() {
    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingToggle.checked);
  });

  test('PreloadPagesDisabled', function() {
    speedPage.$.preloadingToggle.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.DISABLED,
        speedPage.getPref('net.network_prediction_options').value);
    assertFalse(speedPage.$.preloadingToggle.checked);
  });

  test('PreloadPagesStandard', function() {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    speedPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.DISABLED);

    speedPage.$.preloadingToggle.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesStandardFromExtended', async () => {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    speedPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.EXTENDED);

    speedPage.$.preloadingStandard.click();
    await eventToPromise('change', speedPage.$.preloadingRadioGroup);

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesExtended', async () => {
    speedPage.$.preloadingExtended.click();
    await eventToPromise('change', speedPage.$.preloadingRadioGroup);

    assertEquals(
        NetworkPredictionOptions.EXTENDED,
        speedPage.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingExtended.checked);
    assertTrue(speedPage.$.preloadingExtended.expanded);
  });

  test('PreloadPagesStandardExpand', async function() {
    // By default, the preloadingStandard option will be selected and collapsed.
    assertFalse(speedPage.$.preloadingStandard.expanded);

    const expandButton = speedPage.$.preloadingStandard.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingStandard.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesExtendedExpand', async function() {
    assertFalse(speedPage.$.preloadingExtended.expanded);

    const expandButton = speedPage.$.preloadingExtended.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingExtended.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingExtended.expanded);
  });
});

suite('CpuPerformanceOverride', function() {
  let speedPage: SpeedPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let settingsPrivate: FakeSettingsPrivate;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      cpuPerformanceEnabled: true,
    });

    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    performanceBrowserProxy.setCpuPerformanceInfo({
      hardwareTier: 2,
      model: 'Intel Core i7',
      cores: 8,
    });
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: NetworkPredictionOptions.STANDARD,
      },
      {
        key: 'cpu_performance_tier_override',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: -1,
      },
    ];
    const prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    settingsPrivate = prefsBrowserProxy.fakeApi;

    CrSettingsPrefs.resetForTesting();
    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(settingsPrivate);

    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();
    await CrSettingsPrefs.initialized;

    speedPage = document.createElement('settings-speed-page');
    speedPage.prefs = settingsPrefs.prefs!;
    fakeDataBind(settingsPrefs, speedPage, 'prefs');
    document.body.appendChild(speedPage);
    await performanceBrowserProxy.whenCalled('getCpuPerformanceInfo');
    await flushTasks();
  });

  test('HardwareInfoPresent', function() {
    const secondary =
        speedPage.shadowRoot!.querySelector('#cpuPerformanceInfo');

    assertTrue(!!secondary);
    const text = secondary.textContent || '';
    assertStringContains(text, 'Intel Core i7');
    assertStringContains(text, '8 cores');
    assertStringContains(text, 'Tier 2: MID');
  });

  test('DropdownSelectionUpdatesPref', async function() {
    const dropdown =
        speedPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#cpuPerformanceOverrideDropdown');
    assertTrue(!!dropdown);

    // Verify that the dropdown is enabled and both the dropdown and the pref
    // are initially -1.
    assertFalse(dropdown.disabled);
    assertEquals('-1', dropdown.$.dropdownMenu.value);
    assertEquals(
        -1,  // no override
        speedPage.getPref('cpu_performance_tier_override').value);

    // Select 'High' (value 3).
    dropdown.$.dropdownMenu.value = '3';
    dropdown.$.dropdownMenu.dispatchEvent(new CustomEvent('change'));
    await flushTasks();

    // Verify that the pref changed.
    assertEquals(
        3,  // 'High'
        speedPage.getPref('cpu_performance_tier_override').value);
  });

  test('DropdownDisabledWhenPolicyActive', async function() {
    const pref = settingsPrivate.prefs['cpu_performance_tier_override'];
    assertTrue(!!pref);
    pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    settingsPrivate.sendPrefChanges(
        [{key: 'cpu_performance_tier_override', value: 4}]);

    const dropdown =
        speedPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#cpuPerformanceOverrideDropdown');
    assertTrue(!!dropdown);

    await flushTasks();
    await microtasksFinished();

    // Verify that the dropdown is disabled and shows the policy indicator.
    assertTrue(dropdown.shadowRoot.querySelector('select')!.disabled);
    assertTrue(!!dropdown.shadowRoot.querySelector('cr-policy-pref-indicator'));

    // Verify the component respects the enforced preference value.
    assertEquals(4, speedPage.getPref('cpu_performance_tier_override').value);

    // Verify the UI displays the enforced value.
    assertEquals('4', dropdown.$.dropdownMenu.value);
  });
});

suite('CpuPerformanceOverrideFeatureDisabled', function() {
  let speedPage: SpeedPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      cpuPerformanceEnabled: false,
    });

    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: NetworkPredictionOptions.STANDARD,
      },
    ];
    const prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    CrSettingsPrefs.resetForTesting();
    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);

    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();
    await CrSettingsPrefs.initialized;

    speedPage = document.createElement('settings-speed-page');
    speedPage.prefs = settingsPrefs.prefs!;
    fakeDataBind(settingsPrefs, speedPage, 'prefs');
    document.body.appendChild(speedPage);
    await flushTasks();
  });

  test('FeatureDisabled', function() {
    // Verify that the setting is missing.
    const section =
        speedPage.shadowRoot!.querySelector('#cpuPerformanceOverrideDropdown');
    assertNull(section);
  });
});
