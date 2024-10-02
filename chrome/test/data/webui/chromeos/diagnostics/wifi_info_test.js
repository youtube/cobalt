// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {getSignalStrength} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeDisconnectedWifiNetwork, fakeWifiNetwork, fakeWiFiStateProperties} from 'chrome://diagnostics/fake_data.js';
import {Network, SecurityType, WiFiStateProperties} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {WifiInfoElement} from 'chrome://diagnostics/wifi_info.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDataPointHasExpectedHeaderAndValue, assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

suite('wifiInfoTestSuite', function() {
  /** @type {?WifiInfoElement} */
  let wifiInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    wifiInfoElement.remove();
    wifiInfoElement = null;
  });

  /*
   * @param {Network=}
   */
  function initializeWifiInfo(network = fakeWifiNetwork) {
    assertFalse(!!wifiInfoElement);

    // Add the wifi info to the DOM.
    wifiInfoElement =
        /** @type {!WifiInfoElement} */ (document.createElement('wifi-info'));
    assertTrue(!!wifiInfoElement);
    wifiInfoElement.network = network;
    document.body.appendChild(wifiInfoElement);

    return flushTasks();
  }

  /**
   * Helper function provides WiFi network with overridden typeProperties wifi
   * value.
   * @param {!WiFiStateProperties} stateProperties
   * @return {!Network}
   */
  function getWifiNetworkWithWiFiStatePropertiesOf(stateProperties) {
    const wifiTypeProperties =
        Object.assign({}, fakeWiFiStateProperties, stateProperties);
    return /** @type {!Network} */ (Object.assign({}, fakeWifiNetwork, {
      typeProperties: {
        wifi: wifiTypeProperties,
      },
    }));
  }

  test('WifiInfoPopulated', () => {
    const expectedGhz = 5.745;
    return initializeWifiInfo().then(() => {
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#ssid', wifiInfoElement.i18n('networkSsidLabel'),
          `${fakeWifiNetwork.typeProperties.wifi.ssid}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#ipAddress',
          wifiInfoElement.i18n('networkIpAddressLabel'),
          `${fakeWifiNetwork.ipConfig.ipAddress}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#bssid', wifiInfoElement.i18n('networkBssidLabel'),
          `${fakeWifiNetwork.typeProperties.wifi.bssid}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security',
          wifiInfoElement.i18n('networkSecurityLabel'),
          wifiInfoElement.i18n('networkSecurityNoneLabel'));
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#signalStrength',
          wifiInfoElement.i18n('networkSignalStrengthLabel'),
          getSignalStrength(
              fakeWifiNetwork.typeProperties.wifi.signalStrength));
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#channel',
          wifiInfoElement.i18n('networkChannelLabel'),
          `149 (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyConvertibleToChannel', () => {
    // 2412 is the minimum 2.4GHz frequency which can be converted into a valid
    // channel.
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 2412}));
    const expectedGhz = 2.412;
    return initializeWifiInfo(testNetwork).then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `1 (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyNotConvertibleToChannel', () => {
    // 2411 is below the minimum 2.4GHz frequency and cannot be converted into
    // a valid channel.
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 2411}));
    const expectedGhz = 2.411;
    return initializeWifiInfo(testNetwork).then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `? (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyZeroDisplaysEmptyString', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 0}));
    return initializeWifiInfo(testNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#channel'), '');
    });
  });

  test('FrequencyUndefinedDisplaysEmptyString', () => {
    return initializeWifiInfo(fakeDisconnectedWifiNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#channel'), '');
    });
  });

  test('WiFiInfoSecurityWhenNone', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({security: SecurityType.kNone}));
    return initializeWifiInfo(testNetwork).then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      const expectedValue = wifiInfoElement.i18n('networkSecurityNoneLabel');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, expectedValue);
    });
  });

  test('WiFiInfoSecurityWhenWep8021x', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ (
            {security: SecurityType.kWep8021x}));
    return initializeWifiInfo(testNetwork).then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      const expectedValue =
          wifiInfoElement.i18n('networkSecurityWep8021xLabel');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, expectedValue);
    });
  });

  test('WiFiInfoSecurityWhenWepPsk', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({security: SecurityType.kWepPsk}));
    return initializeWifiInfo(testNetwork).then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      const expectedValue = wifiInfoElement.i18n('networkSecurityWepPskLabel');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, expectedValue);
    });
  });

  test('WiFiInfoSecurityWhenWpaEap', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({security: SecurityType.kWpaEap}));
    return initializeWifiInfo(testNetwork).then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      const expectedValue = wifiInfoElement.i18n('networkSecurityWpaEapLabel');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, expectedValue);
    });
  });

  test('WiFiInfoSecurityWhenWpaPsk', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({security: SecurityType.kWpaPsk}));
    return initializeWifiInfo(testNetwork).then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      const expectedValue = wifiInfoElement.i18n('networkSecurityWpaPskLabel');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, expectedValue);
    });
  });

  test('SignalStrengthZeroDisplaysEmptyString', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({signalStrength: 0}));
    return initializeWifiInfo(testNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#signalStrength'), '');
    });
  });

  test('SignalStrengthOneDisplaysEmptyString', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({signalStrength: 1}));
    return initializeWifiInfo(testNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#signalStrength'), '');
    });
  });
});
