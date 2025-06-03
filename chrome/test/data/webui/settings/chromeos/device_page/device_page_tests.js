// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {crosAudioConfigMojom, DevicePageBrowserProxyImpl, fakeCrosAudioConfig, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads, IdleBehavior, LidClosedBehavior, NoteAppLockScreenSupport, Router, routes, setCrosAudioConfigForTesting, setDisplayApiForTesting, setInputDeviceSettingsProviderForTesting, StorageSpaceState} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

/** @enum {string} */
const TestNames = {
  DevicePage: 'device page',
  Audio: 'audio',
  Display: 'display',
  GraphicsTablet: 'graphics tablet',
  Keyboard: 'keyboard',
  PerDeviceMouse: 'per-device mouse',
  PerDeviceTouchpad: 'per-device touchpad',
  NightLight: 'night light',
  PerDeviceKeyboard: 'per-device keyboard',
  Pointers: 'pointers',
  PerDevicePointingStick: 'per-device pointing stick',
  Power: 'power',
  Storage: 'storage',
  Stylus: 'stylus',
  KeyboardArrangementDisabled: 'arrow_key_arrangement_disabled',
};

suite('SettingsDevicePage', function() {
  /** @type {!SettingsDevicePage|undefined} */
  let devicePage;

  /** @type {!FakeSystemDisplay|undefined} */
  let fakeSystemDisplay;

  suiteSetup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function(done) {
    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay);

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.BASIC);

    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
    setDeviceSplitEnabled(true);
    // Allow the light DOM to be distributed to os-settings-animated-pages.
    setTimeout(done);
  });

  async function init() {
    // await is necessary in order for setup() to complete.
    await flushTasks();
    devicePage = document.createElement('settings-device-page');
    devicePage.prefs = getFakePrefs();
    document.body.appendChild(devicePage);
    flush();
  }

  /** @return {!Promise<!HTMLElement>} */
  function showAndGetDeviceSubpage(subpage, expectedRoute) {
    const row =
        assert(devicePage.shadowRoot.querySelector(`#main #${subpage}Row`));
    row.click();
    assertEquals(expectedRoute, Router.getInstance().currentRoute);
    const page = devicePage.shadowRoot.querySelector('settings-' + subpage);
    assert(page);
    return Promise.resolve(page);
  }

  /**
   * @param {!Array<!IdleBehavior>} possibleAcIdleBehaviors
   * @param {!Array<!IdleBehavior>} possibleBatteryIdleBehaviors
   * @param {IdleBehavior} currAcIdleBehavior
   * @param {IdleBehavior} currBatteryIdleBehavior
   * @param {boolean} acIdleManaged
   * @param {boolean} batteryIdleManaged
   * @param {LidClosedBehavior} lidClosedBehavior
   * @param {boolean} lidClosedControlled
   * @param {boolean} hasLid
   * @param {boolean} adaptiveCharging
   * @param {boolean} adaptiveChargingManaged
   */
  function sendPowerManagementSettings(
      possibleAcIdleBehaviors, possibleBatteryIdleBehaviors,
      currentAcIdleBehavior, currentBatteryIdleBehavior, acIdleManaged,
      batteryIdleManaged, lidClosedBehavior, lidClosedControlled, hasLid,
      adaptiveCharging, adaptiveChargingManaged, batterySaverFeatureEnabled) {
    webUIListenerCallback('power-management-settings-changed', {
      possibleAcIdleBehaviors,
      possibleBatteryIdleBehaviors,
      currentAcIdleBehavior,
      currentBatteryIdleBehavior,
      acIdleManaged,
      batteryIdleManaged,
      lidClosedBehavior,
      lidClosedControlled,
      hasLid,
      adaptiveCharging,
      adaptiveChargingManaged,
      batterySaverFeatureEnabled,
    });
    flush();
  }

  /**
   * @param {!HTMLElement} select
   * @param {!value} string
   */
  function selectValue(select, value) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  /**
   * @param {!HTMLElement} pointersPage
   * @param {boolean} expected
   */
  function expectReverseScrollValue(pointersPage, expected) {
    const reverseScrollToggle =
        pointersPage.shadowRoot.querySelector('#enableReverseScrollingToggle');
    assertEquals(expected, reverseScrollToggle.checked);
    assertEquals(
        expected, devicePage.prefs.settings.touchpad.natural_scroll.value);
  }

  /**
   * Checks that the deep link to a setting focuses the correct element.
   * @param {!Route} route
   * @param {!string} settingId
   * @param {!Element} deepLinkElement The element that should be focused by
   *                                   the deep link
   * @param {!string} elementDesc A human-readable description of the element,
   *                              for assertion messages
   */
  async function checkDeepLink(route, settingId, deepLinkElement, elementDesc) {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(route, params);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `${elementDesc} should be focused for settingId=${settingId}.`);
  }

  /**
   * Set enableInputDeviceSettingsSplit feature flag to true for split tests.
   * @param {!boolean} isEnabled
   */
  function setDeviceSplitEnabled(isEnabled) {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: isEnabled,
    });
  }

  /**
   * Set enablePeripheralCustomization feature flag to true for split tests.
   * @param {!boolean} isEnabled
   */
  function setPeripheralCustomizationEnabled(isEnabled) {
    loadTimeData.overrideValues({
      enablePeripheralCustomization: isEnabled,
    });
  }

  test(assert(TestNames.DevicePage), async function() {
    const provider = new FakeInputDeviceSettingsProvider();
    setInputDeviceSettingsProviderForTesting(provider);
    provider.setFakeMice(fakeMice);
    provider.setFakePointingSticks(fakePointingSticks);
    provider.setFakeTouchpads(fakeTouchpads);
    provider.setFakeGraphicsTablets(fakeGraphicsTablets);

    await init();
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#displayRow')));
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#audioRow')));

    // enableInputDeviceSettingsSplit feature flag by default is turned on.
    assertTrue(
        isVisible(devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));
    assertTrue(isVisible(
        devicePage.shadowRoot.querySelector('#perDeviceKeyboardRow')));
    assertFalse(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));
    assertFalse(isVisible(devicePage.shadowRoot.querySelector('#keyboardRow')));

    // enablePeripheralCustomization feature flag by default is turned on.
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#tabletRow')));

    // Turn off the enableInputDeviceSettingsSplit feature flag.
    setDeviceSplitEnabled(false);
    await init();
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#keyboardRow')));

    webUIListenerCallback('has-mouse-changed', false);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));

    webUIListenerCallback('has-pointing-stick-changed', false);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));

    webUIListenerCallback('has-touchpad-changed', false);
    await flushTasks();
    assertFalse(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));

    webUIListenerCallback('has-mouse-changed', true);
    await flushTasks();
    assertTrue(isVisible(devicePage.shadowRoot.querySelector('#pointersRow')));

    // Turn off the enablePeripheralCustomization feature flag.
    setPeripheralCustomizationEnabled(false);
    await init();
    assertFalse(isVisible(devicePage.shadowRoot.querySelector('#tabletRow')));
  });

  test('per-device-mouse row visibility', async function() {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(
        isVisible(devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));
  });

  test(
      'per-device-mouse row visibility based on devices connected',
      async function() {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakeMice(fakeMice);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));

        provider.setFakeMice([]);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));

        provider.setFakeMice(fakeMice);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-mouse-changed', true);
        await init();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));

        webUIListenerCallback('has-mouse-changed', false);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceMouseRow')));
      });

  test('per-device-touchpad row visibility', async function() {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));
  });

  test(
      'per-device-touchpad row visiblity based on connected devices',
      async function() {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakeTouchpads(fakeTouchpads);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));

        provider.setFakeTouchpads([]);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));

        provider.setFakeTouchpads(fakeTouchpads);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-touchpad-changed', true);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));

        webUIListenerCallback('has-touchpad-changed', false);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDeviceTouchpadRow')));
      });

  test('per-device-pointing-stick row visibility', async function() {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));
  });

  test(
      'per-device-pointing-stick row visibility based on devices connected',
      async function() {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);

        // Tests with flag on.
        setDeviceSplitEnabled(true);
        await init();

        provider.setFakePointingSticks(fakePointingSticks);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));

        provider.setFakePointingSticks([]);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));

        provider.setFakePointingSticks(fakePointingSticks);
        await flushTasks();
        assertTrue(isVisible(
            devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));

        // Tests with flag off.
        setDeviceSplitEnabled(false);
        await init();

        webUIListenerCallback('has-pointing-stick-stick-changed', true);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));

        webUIListenerCallback('has-pointing-stick-changed', false);
        await flushTasks();
        assertFalse(isVisible(
            devicePage.shadowRoot.querySelector('#perDevicePointingStickRow')));
      });

  test('per-device-keyboard row visibility', async function() {
    setDeviceSplitEnabled(false);
    await init();
    assertFalse(isVisible(
        devicePage.shadowRoot.querySelector('#perDeviceKeyboardRow')));
  });

  test(
      'graphics tablet row visibility based on devices connected',
      async function() {
        const provider = new FakeInputDeviceSettingsProvider();
        setInputDeviceSettingsProviderForTesting(provider);
        provider.setFakeGraphicsTablets(fakeGraphicsTablets);

        // Tests with flag on.
        setPeripheralCustomizationEnabled(true);
        await init();

        assertTrue(
            isVisible(devicePage.shadowRoot.querySelector('#tabletRow')));

        provider.setFakeGraphicsTablets([]);
        await flushTasks();
        assertFalse(
            isVisible(devicePage.shadowRoot.querySelector('#tabletRow')));

        provider.setFakeGraphicsTablets(fakeGraphicsTablets);
        await flushTasks();
        assertTrue(
            isVisible(devicePage.shadowRoot.querySelector('#tabletRow')));
      });

  test('graphics tablet subpage navigates back to device page', async () => {
    const provider = new FakeInputDeviceSettingsProvider();
    setInputDeviceSettingsProviderForTesting(provider);
    provider.setFakeGraphicsTablets(fakeGraphicsTablets);

    // Tests with flag on.
    setPeripheralCustomizationEnabled(true);
    await init();

    const row = assert(devicePage.shadowRoot.querySelector(`#tabletRow`));
    row.click();
    assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);

    provider.setFakeGraphicsTablets([]);
    await flushTasks();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);
  });

  suite(assert(TestNames.PerDeviceKeyboard), function() {
    let perDeviceKeyboardPage;
    let inputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeKeyboards(fakeKeyboards);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async function() {
      await init();
      const row = assert(
          devicePage.shadowRoot.querySelector(`#main #perDeviceKeyboardRow`));
      row.click();
      assertEquals(
          routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
      const page =
          devicePage.shadowRoot.querySelector('settings-per-device-keyboard');
      assert(page);
      perDeviceKeyboardPage = page;
    });

    test('per-device keyboard subpage visibility', function() {
      assertEquals(
          routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
    });

    test('per-device keyboard page populated', function() {
      const connectedKeyboards = perDeviceKeyboardPage.keyboards;
      assertTrue(!!connectedKeyboards);
      assertDeepEquals(connectedKeyboards, fakeKeyboards);
    });
  });

  suite(assert(TestNames.Audio), function() {
    let audioPage;

    /** @type {?FakeCrosAudioConfig} */
    let crosAudioConfig;

    // Static test audio system properties.
    /** @type {!AudioSystemProperties} */
    const maxVolumePercentFakeAudioSystemProperties = {
      outputVolumePercent: 100,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [],
    };

    /** @type {!AudioSystemProperties} */
    const minVolumePercentFakeAudioSystemProperties = {
      outputVolumePercent: 0,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [],
    };

    /** @type {!AudioSystemProperties} */
    const mutedByUserFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],

      /** @type {!MuteState} */
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
    };

    /** @type {!AudioSystemProperties} */
    const mutedByPolicyFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByPolicy,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],

      /** @type {!MuteState} */
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByPolicy,

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
    };

    /** @type {!AudioSystemProperties} */
    const mutedExternallyFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.defaultFakeSpeaker,
        fakeCrosAudioConfig.defaultFakeMicJack,
      ],

      /** @type {!MuteState} */
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
    };

    /** @type {!AudioSystemProperties} */
    const emptyOutputDevicesFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
    };

    /** @type {!AudioSystemProperties} */
    const emptyInputDevicesFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.fakeSpeakerActive,
        fakeCrosAudioConfig.fakeMicJackInactive,
      ],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [],
    };

    /** @type {!AudioSystemProperties} */
    const activeSpeakerFakeAudioSystemProperties = {
      outputVolumePercent: 75,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [
        fakeCrosAudioConfig.fakeSpeakerActive,
        fakeCrosAudioConfig.fakeMicJackInactive,
      ],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [],
    };

    /** @type {!AudioSystemProperties} */
    const noiseCancellationNotSupportedAudioSystemProperties = {
      outputVolumePercent: 0,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],
    };

    /** @type {!AudioSystemProperties} */
    const muteByHardwareAudioSystemProperties = {
      outputVolumePercent: 0,

      /** @type {!MuteState} */
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,

      /** @type {!Array<!AudioDevice>} */
      outputDevices: [],

      /** @type {!Array<!AudioDevice>} */
      inputDevices: [
        fakeCrosAudioConfig.fakeInternalMicActive,
      ],

      inputMuteState: crosAudioConfigMojom.MuteState.kMutedExternally,
    };

    /**
     * Simulates clicking at a given point on cr-slider element.
     * @param {string} crSliderSelector
     * @param {number} percent
     * @return {Promise}
     */
    async function simulateSliderClicked(crSliderSelector, percent) {
      /** @type {!CrSliderElement} */
      const crSlider = audioPage.shadowRoot.querySelector(crSliderSelector);
      assertTrue(isVisible(crSlider));
      assertFalse(crSlider.disabled);
      const rect = crSlider.$.container.getBoundingClientRect();
      crSlider.dispatchEvent(new PointerEvent('pointerdown', {
        buttons: 1,
        pointerId: 1,
        clientX: rect.left + ((percent / 100) * rect.width),
      }));
      return flushTasks();
    }

    /**
     * Simulates pressing left arrow key while focused on cr-slider element.
     * @param {!CrSliderElement} crSlider
     */
    function pressArrowRight(crSlider) {
      pressAndReleaseKeyOn(crSlider, 39, [], 'ArrowRight');
    }

    /**
     * Simulates pressing right arrow key while focused on cr-slider element.
     * @param {!CrSliderElement} crSlider
     */
    function pressArrowLeft(crSlider) {
      pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
    }

    setup(async function() {
      await init();

      // FakeAudioConfig must be set before audio subpage is loaded.
      crosAudioConfig = new fakeCrosAudioConfig.FakeCrosAudioConfig();
      setCrosAudioConfigForTesting(crosAudioConfig);
      // Ensure data reset to fresh state.
      crosAudioConfig.setAudioSystemProperties(
          {...fakeCrosAudioConfig.defaultFakeAudioSystemProperties});
      return showAndGetDeviceSubpage('audio', routes.AUDIO)
          .then(function(page) {
            audioPage = page;
            return flushTasks();
          });
    });

    test('subpage visibility', function() {
      assertEquals(routes.AUDIO, Router.getInstance().currentRoute);
      assertTrue(
          isVisible(audioPage.shadowRoot.querySelector('#audioOutputTitle')));
      assertTrue(isVisible(
          audioPage.shadowRoot.querySelector('#audioOutputSubsection')));
      assertTrue(
          isVisible(audioPage.shadowRoot.querySelector('#audioInputSection')));
      const inputSectionHeader =
          audioPage.shadowRoot.querySelector('#audioInputTitle');
      assertTrue(isVisible(inputSectionHeader));
      assertEquals('Input', inputSectionHeader.textContent.trim());
      const inputDeviceSubsectionHeader =
          audioPage.shadowRoot.querySelector('#audioInputDeviceLabel');
      assertTrue(isVisible(inputDeviceSubsectionHeader));
      assertEquals(
          'Microphone', inputDeviceSubsectionHeader.textContent.trim());
      const inputDeviceSubsectionDropdown =
          audioPage.shadowRoot.querySelector('#audioInputDeviceDropdown');
      assertTrue(isVisible(inputDeviceSubsectionDropdown));
      const inputGainSubsectionHeader =
          audioPage.shadowRoot.querySelector('#audioInputGainLabel');
      assertTrue(isVisible(inputGainSubsectionHeader), 'audioInputGainLabel');
      assertEquals('Volume', inputGainSubsectionHeader.textContent.trim());
      const inputVolumeButton =
          audioPage.shadowRoot.querySelector('#audioInputGainMuteButton');
      assertTrue(isVisible(inputVolumeButton), 'audioInputGainMuteButton');
      const inputVolumeSlider =
          audioPage.shadowRoot.querySelector('#audioInputGainVolumeSlider');
      assertTrue(isVisible(inputVolumeSlider), 'audioInputGainVolumeSlider');
      const noiseCancellationSubsectionHeader =
          audioPage.shadowRoot.querySelector(
              '#audioInputNoiseCancellationLabel');
      assertTrue(isVisible(noiseCancellationSubsectionHeader));
      assertEquals(
          'Noise cancellation',
          noiseCancellationSubsectionHeader.textContent.trim());
      const noiseCancellationToggle = audioPage.shadowRoot.querySelector(
          '#audioInputNoiseCancellationToggle');
      assertTrue(isVisible(noiseCancellationToggle));
    });

    test('output volume mojo test', async function() {
      const outputVolumeSlider =
          audioPage.shadowRoot.querySelector('#outputVolumeSlider');

      // Test default properties.
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties
              .outputVolumePercent,
          outputVolumeSlider.value);

      // Test min volume case.
      crosAudioConfig.setAudioSystemProperties(
          minVolumePercentFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          minVolumePercentFakeAudioSystemProperties.outputVolumePercent,
          outputVolumeSlider.value);

      // Test max volume case.
      crosAudioConfig.setAudioSystemProperties(
          maxVolumePercentFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          maxVolumePercentFakeAudioSystemProperties.outputVolumePercent,
          outputVolumeSlider.value);
    });

    test('simulate setting output volume slider mojo test', async function() {
      const sliderSelector = '#outputVolumeSlider';
      const outputSlider = audioPage.shadowRoot.querySelector(sliderSelector);
      const outputMuteButton =
          audioPage.shadowRoot.querySelector('#audioOutputMuteButton');

      // Test clicking to min volume case.
      const minOutputVolumePercent = 0;
      await simulateSliderClicked(sliderSelector, minOutputVolumePercent);
      assertEquals(
          minOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent,
      );
      assertEquals('settings20:volume-zero', outputMuteButton.ironIcon);

      // Test clicking to max volume case.
      const maxOutputVolumePercent = 100;
      await simulateSliderClicked(sliderSelector, maxOutputVolumePercent);
      assertEquals(
          maxOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent,
      );
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Test clicking to non-boundary volume case.
      const nonBoundaryOutputVolumePercent = 50;
      await simulateSliderClicked(
          sliderSelector, nonBoundaryOutputVolumePercent);
      assertEquals(
          nonBoundaryOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent,
      );
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Ensure value clamps to min.
      outputSlider.value = -1;
      outputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          minOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent);
      assertEquals('settings20:volume-zero', outputMuteButton.ironIcon);

      // Ensure value clamps to max.
      outputSlider.value = 101;
      outputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          maxOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent);
      assertEquals('settings20:volume-up', outputMuteButton.ironIcon);

      // Test clicking to a small icon volume case.
      const smallIconOutputVolumePercent = 10;
      await simulateSliderClicked(sliderSelector, smallIconOutputVolumePercent);
      assertEquals(
          smallIconOutputVolumePercent,
          audioPage.audioSystemProperties_.outputVolumePercent,
      );
      assertEquals('settings20:volume-down', outputMuteButton.ironIcon);
    });

    test('output mute state changes slider disabled state', async function() {
      const outputVolumeSlider =
          audioPage.shadowRoot.querySelector('#outputVolumeSlider');

      // Test default properties.
      assertFalse(audioPage.getIsOutputMutedForTest());
      assertFalse(outputVolumeSlider.disabled);

      // Test muted by user case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByUserFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(audioPage.getIsOutputMutedForTest());
      assertFalse(outputVolumeSlider.disabled);

      // Test muted by policy case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(audioPage.getIsOutputMutedForTest());
      assertTrue(outputVolumeSlider.disabled);
    });

    test('output device mojo test', async function() {
      const outputDeviceDropdown =
          audioPage.shadowRoot.querySelector('#audioOutputDeviceDropdown');

      // Test default properties.
      assertEquals(
          fakeCrosAudioConfig.defaultFakeMicJack.id,
          BigInt(outputDeviceDropdown.value));
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties.outputDevices
              .length,
          outputDeviceDropdown.length);

      // Test empty output devices case.
      crosAudioConfig.setAudioSystemProperties(
          emptyOutputDevicesFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(!outputDeviceDropdown.value);
      assertEquals(
          emptyOutputDevicesFakeAudioSystemProperties.outputDevices.length,
          outputDeviceDropdown.length);

      // If the output devices are empty, the output section should be hidden.
      const outputDeviceSection = audioPage.shadowRoot.querySelector('#output');
      assertFalse(isVisible(outputDeviceSection));
      const inputDeviceSection = audioPage.shadowRoot.querySelector('#input');
      assertTrue(isVisible(inputDeviceSection));

      // Test active speaker case.
      crosAudioConfig.setAudioSystemProperties(
          activeSpeakerFakeAudioSystemProperties);
      await flushTasks();

      assertEquals(
          fakeCrosAudioConfig.fakeSpeakerActive.id,
          BigInt(outputDeviceDropdown.value));
      assertEquals(
          activeSpeakerFakeAudioSystemProperties.outputDevices.length,
          outputDeviceDropdown.length);
    });

    test('simulate setting active output device', async function() {
      // Get dropdown.
      /** @type {!HTMLSelectElement}*/
      const outputDeviceDropdown =
          audioPage.shadowRoot.querySelector('#audioOutputDeviceDropdown');

      // Verify selected is active device.
      const expectedInitialSelectionId =
          `${fakeCrosAudioConfig.defaultFakeMicJack.id}`;
      assertEquals(expectedInitialSelectionId, outputDeviceDropdown.value);

      // change active device.
      outputDeviceDropdown.selectedIndex = 0;
      outputDeviceDropdown.dispatchEvent(
          new CustomEvent('change', {bubbles: true}));
      await flushTasks();

      // Verify selected updated to latest active device.
      const expectedUpdatedSelectionId =
          `${fakeCrosAudioConfig.defaultFakeSpeaker.id}`;
      assertEquals(expectedUpdatedSelectionId, outputDeviceDropdown.value);
      const nextActiveDevice =
          audioPage.audioSystemProperties_.outputDevices.find(
              device =>
                  device.id === fakeCrosAudioConfig.defaultFakeSpeaker.id);
      assertTrue(nextActiveDevice.isActive);
    });

    test('input device mojo test', async function() {
      const inputDeviceDropdown =
          audioPage.shadowRoot.querySelector('#audioInputDeviceDropdown');
      assertTrue(!!inputDeviceDropdown);

      // Test default properties.
      assertEquals(
          `${fakeCrosAudioConfig.fakeInternalFrontMic.id}`,
          inputDeviceDropdown.value);
      assertEquals(
          fakeCrosAudioConfig.defaultFakeAudioSystemProperties.inputDevices
              .length,
          inputDeviceDropdown.length);

      // Test empty input devices case.
      crosAudioConfig.setAudioSystemProperties(
          emptyInputDevicesFakeAudioSystemProperties);
      await flushTasks();
      assertTrue(!inputDeviceDropdown.value);
      assertEquals(
          emptyInputDevicesFakeAudioSystemProperties.inputDevices.length,
          inputDeviceDropdown.length);

      // If the input devices are empty, the input section should be hidden.
      const inputDeviceSection = audioPage.shadowRoot.querySelector('#input');
      assertFalse(isVisible(inputDeviceSection));
      const outputDeviceSection = audioPage.shadowRoot.querySelector('#output');
      assertTrue(isVisible(outputDeviceSection));
    });

    test('simulate setting active input device', async function() {
      // Get dropdown.
      /** @type {!HTMLSelectElement}*/
      const inputDeviceDropdown =
          audioPage.shadowRoot.querySelector('#audioInputDeviceDropdown');

      // Verify selected is active device.
      const expectedInitialSelectionId =
          `${fakeCrosAudioConfig.fakeInternalFrontMic.id}`;
      assertEquals(expectedInitialSelectionId, inputDeviceDropdown.value);

      // change active device.
      inputDeviceDropdown.selectedIndex = 1;
      inputDeviceDropdown.dispatchEvent(
          new CustomEvent('change', {bubbles: true}));
      await flushTasks();

      // Verify selected updated to latest active device.
      const expectedUpdatedSelectionId =
          `${fakeCrosAudioConfig.fakeBluetoothMic.id}`;
      assertEquals(expectedUpdatedSelectionId, inputDeviceDropdown.value);
      const nextActiveDevice =
          audioPage.audioSystemProperties_.inputDevices.find(
              device => device.id === fakeCrosAudioConfig.fakeBluetoothMic.id);
      assertTrue(nextActiveDevice.isActive);
    });

    test('simulate mute output', async function() {
      assertEquals(
          crosAudioConfigMojom.MuteState.kNotMuted,
          audioPage.audioSystemProperties_.outputMuteState);
      assertFalse(audioPage.isOutputMuted_);

      const outputMuteButton =
          audioPage.shadowRoot.querySelector('#audioOutputMuteButton');
      assertTrue(isVisible(outputMuteButton));
      outputMuteButton.click();
      await flushTasks();

      assertEquals(
          crosAudioConfigMojom.MuteState.kMutedByUser,
          audioPage.audioSystemProperties_.outputMuteState);
      assertTrue(audioPage.isOutputMuted_);
      assertEquals('settings20:volume-up-off', outputMuteButton.ironIcon);

      outputMuteButton.click();
      await flushTasks();

      assertEquals(
          crosAudioConfigMojom.MuteState.kNotMuted,
          audioPage.audioSystemProperties_.outputMuteState);
      assertFalse(audioPage.isOutputMuted_);
    });

    test('simulate input mute button press test', async function() {
      const inputMuteButton =
          audioPage.shadowRoot.querySelector('#audioInputGainMuteButton');

      assertFalse(audioPage.getIsInputMutedForTest());
      assertEquals('cr:mic', inputMuteButton.ironIcon);

      inputMuteButton.click();
      await flushTasks();

      assertTrue(audioPage.getIsInputMutedForTest());
      assertEquals('settings:mic-off', inputMuteButton.ironIcon);
    });

    test('simulate setting input gain slider', async function() {
      const sliderSelector = '#audioInputGainVolumeSlider';
      const inputSlider = audioPage.shadowRoot.querySelector(sliderSelector);
      assertTrue(isVisible(inputSlider));
      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, inputSlider.value);

      const minimumValue = 0;
      await simulateSliderClicked(sliderSelector, minimumValue);

      assertEquals(minimumValue, inputSlider.value);
      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, inputSlider.value);
      const maximumValue = 100;
      await simulateSliderClicked(sliderSelector, maximumValue);

      assertEquals(maximumValue, inputSlider.value);
      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, inputSlider.value);
      const middleValue = 50;
      await simulateSliderClicked(sliderSelector, middleValue);

      assertEquals(middleValue, inputSlider.value);
      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, inputSlider.value);

      // Ensure value clamps to min.
      inputSlider.value = -1;
      inputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, minimumValue);

      // Ensure value clamps to min.
      inputSlider.value = 101;
      inputSlider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
      await flushTasks();

      assertEquals(
          audioPage.audioSystemProperties_.inputGainPercent, maximumValue);
    });

    test('simulate noise cancellation', async function() {
      const noiseCancellationSubsection = audioPage.shadowRoot.querySelector(
          '#audioInputNoiseCancellationSubsection');
      const noiseCancellationToggle = audioPage.shadowRoot.querySelector(
          '#audioInputNoiseCancellationToggle');

      assertTrue(isVisible(noiseCancellationSubsection));
      assertFalse(noiseCancellationToggle.checked);

      await noiseCancellationToggle.click();
      await flushTasks();

      assertTrue(isVisible(noiseCancellationSubsection));
      assertTrue(noiseCancellationToggle.checked);

      crosAudioConfig.setAudioSystemProperties(
          noiseCancellationNotSupportedAudioSystemProperties);
      await flushTasks();

      assertFalse(
          isVisible(noiseCancellationSubsection),
      );
    });

    test('simulate input muted by hardware', async function() {
      const muteSelector = '#audioInputGainMuteButton';
      const inputMuteButton = audioPage.shadowRoot.querySelector(muteSelector);
      const sliderSelector = '#audioInputGainVolumeSlider';
      const inputSlider = audioPage.shadowRoot.querySelector(sliderSelector);
      assertFalse(inputMuteButton.disabled);
      assertFalse(inputSlider.disabled);
      assertFalse(audioPage.getIsInputMutedForTest());

      crosAudioConfig.setAudioSystemProperties(
          muteByHardwareAudioSystemProperties);
      await flushTasks();

      assertTrue(inputMuteButton.disabled);
      assertTrue(inputSlider.disabled);
      assertTrue(audioPage.getIsInputMutedForTest());
    });

    test('simulate output mute-by-policy', async function() {
      const enterpriseIconSelector = '#audioOutputMuteByPolicyIndicator';
      assertFalse(isVisible(
          audioPage.shadowRoot.querySelector(enterpriseIconSelector)));
      const outputMuteButtonSelector = '#audioOutputMuteButton';
      const outputMuteButton =
          audioPage.shadowRoot.querySelector(outputMuteButtonSelector);
      assertFalse(outputMuteButton.disabled);
      const outputSliderSelector = '#outputVolumeSlider';
      const outputSlider =
          audioPage.shadowRoot.querySelector(outputSliderSelector);
      assertFalse(outputSlider.disabled);

      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();

      assertTrue(isVisible(
          audioPage.shadowRoot.querySelector(enterpriseIconSelector)));
      assertTrue(outputMuteButton.disabled);
      assertTrue(outputSlider.disabled);
    });

    test('noise cancellation called twice with same value', async function() {
      const mockController = new MockController();
      const setNoiseCancellationEnabled = mockController.createFunctionMock(
          crosAudioConfig, 'setNoiseCancellationEnabled');

      assertEquals(
          /* expected_call_count */ 0,
          setNoiseCancellationEnabled.calls_.length);

      crosAudioConfig.setAudioSystemProperties(
          {...fakeCrosAudioConfig.defaultFakeAudioSystemProperties});
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 0,
          setNoiseCancellationEnabled.calls_.length);

      crosAudioConfig.setAudioSystemProperties(
          noiseCancellationNotSupportedAudioSystemProperties);
      await flushTasks();

      assertEquals(
          /* expected_call_count */ 1,
          setNoiseCancellationEnabled.calls_.length);
    });

    test('slider keypress correct increments', async function() {
      // The audio sliders are expected to increment in intervals of 10.
      const outputVolumeSlider =
          audioPage.shadowRoot.querySelector('#outputVolumeSlider');
      assertEquals(75, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(85, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(95, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(100, outputVolumeSlider.value);
      pressArrowRight(outputVolumeSlider);
      assertEquals(100, outputVolumeSlider.value);
      pressArrowLeft(outputVolumeSlider);
      assertEquals(90, outputVolumeSlider.value);
      pressArrowLeft(outputVolumeSlider);
      assertEquals(80, outputVolumeSlider.value);

      const inputVolumeSlider =
          audioPage.shadowRoot.querySelector('#audioInputGainVolumeSlider');
      assertEquals(87, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(97, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(100, inputVolumeSlider.value);
      pressArrowRight(inputVolumeSlider);
      assertEquals(100, inputVolumeSlider.value);
      pressArrowLeft(inputVolumeSlider);
      assertEquals(90, inputVolumeSlider.value);
      pressArrowLeft(inputVolumeSlider);
      assertEquals(80, inputVolumeSlider.value);
    });

    test('mute state updates tooltips', async function() {
      const outputMuteTooltip =
          audioPage.shadowRoot.querySelector('#audioOutputMuteButtonTooltip');
      const inputMuteTooltip =
          audioPage.shadowRoot.querySelector('#audioInputMuteButtonTooltip');

      // Default state should be unmuted so show the toggle mute tooltip.
      assertEquals(
          loadTimeData.getString('audioToggleToMuteTooltip'),
          outputMuteTooltip.textContent.trim());
      assertEquals(
          loadTimeData.getString('audioToggleToMuteTooltip'),
          inputMuteTooltip.textContent.trim());

      // Test muted by user case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByUserFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioToggleToUnmuteTooltip'),
          outputMuteTooltip.textContent.trim());
      assertEquals(
          loadTimeData.getString('audioToggleToUnmuteTooltip'),
          inputMuteTooltip.textContent.trim());

      // Test muted by policy case.
      crosAudioConfig.setAudioSystemProperties(
          mutedByPolicyFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioMutedByPolicyTooltip'),
          outputMuteTooltip.textContent.trim());
      assertEquals(
          loadTimeData.getString('audioMutedByPolicyTooltip'),
          inputMuteTooltip.textContent.trim());

      // Test muted externally case.
      crosAudioConfig.setAudioSystemProperties(
          mutedExternallyFakeAudioSystemProperties);
      await flushTasks();
      assertEquals(
          loadTimeData.getString('audioMutedExternallyTooltip'),
          outputMuteTooltip.textContent.trim());
      assertEquals(
          loadTimeData.getString('audioMutedExternallyTooltip'),
          inputMuteTooltip.textContent.trim());
    });

    test(
        'mute state updates button aria-description and aria-pressed',
        async function() {
          const outputMuteButton =
              audioPage.shadowRoot.querySelector('#audioOutputMuteButton');
          const inputMuteButton =
              audioPage.shadowRoot.querySelector('#audioInputGainMuteButton');

          // Default state should be unmuted so show the toggle mute tooltip.
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelNotMuted'),
              outputMuteButton.ariaDescription.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelNotMuted'),
              inputMuteButton.ariaDescription.trim());
          const ariaNotPressedValue = 'false';
          assertEquals(ariaNotPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaNotPressedValue, inputMuteButton.ariaPressed);

          // Test muted by user case.
          crosAudioConfig.setAudioSystemProperties(
              mutedByUserFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.ariaDescription.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelMuted'),
              inputMuteButton.ariaDescription.trim());
          const ariaPressedValue = 'true';
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);

          // Test muted by policy case.
          crosAudioConfig.setAudioSystemProperties(
              mutedByPolicyFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.ariaDescription.trim());
          assertEquals(
              loadTimeData.getString('audioInputMuteButtonAriaLabelMuted'),
              inputMuteButton.ariaDescription.trim());
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);

          // Test muted externally case.
          crosAudioConfig.setAudioSystemProperties(
              mutedExternallyFakeAudioSystemProperties);
          await flushTasks();
          assertEquals(
              loadTimeData.getString('audioOutputMuteButtonAriaLabelMuted'),
              outputMuteButton.ariaDescription.trim());
          assertEquals(
              loadTimeData.getString(
                  'audioInputMuteButtonAriaLabelMutedByHardwareSwitch'),
              inputMuteButton.ariaDescription.trim());
          assertEquals(ariaPressedValue, outputMuteButton.ariaPressed);
          assertEquals(ariaPressedValue, inputMuteButton.ariaPressed);
        });
  });

  suite(assert(TestNames.PerDeviceMouse), function() {
    let perDeviceMousePage;
    let inputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeMice(fakeMice);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async function() {
      await init();
      const row = assert(
          devicePage.shadowRoot.querySelector(`#main #perDeviceMouseRow`));
      row.click();
      assertEquals(routes.PER_DEVICE_MOUSE, Router.getInstance().currentRoute);
      const page =
          devicePage.shadowRoot.querySelector('settings-per-device-mouse');
      assert(page);
      return Promise.resolve(page).then(function(page) {
        perDeviceMousePage = page;
      });
    });

    test('per-device mouse subpage visibility', function() {
      assertEquals(routes.PER_DEVICE_MOUSE, Router.getInstance().currentRoute);
    });
  });

  suite(assert(TestNames.PerDeviceTouchpad), function() {
    let perDeviceTouchpadPage;
    let inputDeviceSettingsProvider;
    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeTouchpads(fakeTouchpads);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async function() {
      await init();
      const row = assert(
          devicePage.shadowRoot.querySelector(`#main #perDeviceTouchpadRow`));
      row.click();
      assertEquals(
          routes.PER_DEVICE_TOUCHPAD, Router.getInstance().currentRoute);
      const page =
          devicePage.shadowRoot.querySelector('settings-per-device-touchpad');
      assert(page);
      return Promise.resolve(page).then(function(page) {
        perDeviceTouchpadPage = page;
      });
    });

    test('per-device touchpad subpage visibility', function() {
      assertEquals(
          routes.PER_DEVICE_TOUCHPAD, Router.getInstance().currentRoute);
    });
  });

  suite(assert(TestNames.PerDevicePointingStick), function() {
    let perDevicePointingStickPage;
    let inputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakePointingSticks(fakePointingSticks);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async function() {
      await init();
      const row = assert(devicePage.shadowRoot.querySelector(
          `#main #perDevicePointingStickRow`));
      row.click();
      assertEquals(
          routes.PER_DEVICE_POINTING_STICK, Router.getInstance().currentRoute);
      const page = devicePage.shadowRoot.querySelector(
          'settings-per-device-pointing-stick');
      assert(page);
      return Promise.resolve(page).then(function(page) {
        perDevicePointingStickPage = page;
      });
    });

    test('per-device pointing stick subpage visibility', function() {
      assertEquals(
          routes.PER_DEVICE_POINTING_STICK, Router.getInstance().currentRoute);
    });
  });

  suite(assert(TestNames.Pointers), function() {
    let pointersPage;

    setup(async function() {
      setDeviceSplitEnabled(false);
      await init();
      return showAndGetDeviceSubpage('pointers', routes.POINTERS)
          .then(function(page) {
            pointersPage = page;
          });
    });

    test('subpage responds to pointer attach/detach', async function() {
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse')));
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse h2')));
      assertTrue(
          isVisible(pointersPage.shadowRoot.querySelector('#pointingStick')));
      assertTrue(isVisible(
          pointersPage.shadowRoot.querySelector('#pointingStick h2')));
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));
      assertTrue(
          isVisible(pointersPage.shadowRoot.querySelector('#touchpad h2')));

      webUIListenerCallback('has-touchpad-changed', false);
      await flushTasks();
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse')));
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse h2')));
      assertTrue(
          isVisible(pointersPage.shadowRoot.querySelector('#pointingStick')));
      assertTrue(isVisible(
          pointersPage.shadowRoot.querySelector('#pointingStick h2')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#touchpad h2')));

      webUIListenerCallback('has-pointing-stick-changed', false);
      await flushTasks();
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#mouse h2')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#pointingStick')));
      assertFalse(isVisible(
          pointersPage.shadowRoot.querySelector('#pointingStick h2')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));
      assertFalse(
          isVisible(pointersPage.shadowRoot.querySelector('#touchpad h2')));

      webUIListenerCallback('has-mouse-changed', false);
      await flushTasks();
      assertEquals(routes.DEVICE, Router.getInstance().currentRoute);
      assertFalse(
          isVisible(devicePage.shadowRoot.querySelector('#main #pointersRow')));

      webUIListenerCallback('has-touchpad-changed', true);
      await flushTasks();
      assertTrue(
          isVisible(devicePage.shadowRoot.querySelector('#main #pointersRow')));

      return showAndGetDeviceSubpage('pointers', routes.POINTERS)
          .then(function(page) {
            assertFalse(
                isVisible(pointersPage.shadowRoot.querySelector('#mouse')));
            assertFalse(
                isVisible(pointersPage.shadowRoot.querySelector('#mouse h2')));
            assertFalse(isVisible(
                pointersPage.shadowRoot.querySelector('#pointingStick')));
            assertFalse(isVisible(
                pointersPage.shadowRoot.querySelector('#pointingStick h2')));
            assertTrue(
                isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));
            assertFalse(isVisible(
                pointersPage.shadowRoot.querySelector('#touchpad h2')));

            webUIListenerCallback('has-mouse-changed', true);
            assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
            assertTrue(
                isVisible(pointersPage.shadowRoot.querySelector('#mouse')));
            assertTrue(
                isVisible(pointersPage.shadowRoot.querySelector('#mouse h2')));
            assertFalse(isVisible(
                pointersPage.shadowRoot.querySelector('#pointingStick')));
            assertFalse(isVisible(
                pointersPage.shadowRoot.querySelector('#pointingStick h2')));
            assertTrue(
                isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));
            assertTrue(isVisible(
                pointersPage.shadowRoot.querySelector('#touchpad h2')));
          });
    });

    test('mouse', function() {
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#mouse')));

      const slider = assert(
          pointersPage.shadowRoot.querySelector('#mouse settings-slider'));
      assertEquals(4, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 37, [], 'ArrowLeft');
      assertEquals(3, devicePage.prefs.settings.mouse.sensitivity2.value);

      pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
      assertEquals(5, slider.pref.value);
    });

    test('touchpad', function() {
      assertTrue(isVisible(pointersPage.shadowRoot.querySelector('#touchpad')));

      assertTrue(
          pointersPage.shadowRoot.querySelector('#touchpad #enableTapToClick')
              .checked);
      assertFalse(
          pointersPage.shadowRoot.querySelector('#touchpad #enableTapDragging')
              .checked);

      const slider = assert(
          pointersPage.shadowRoot.querySelector('#touchpad settings-slider'));
      assertEquals(3, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 39 /* right */, [],
          'ArrowRight');
      assertEquals(4, devicePage.prefs.settings.touchpad.sensitivity2.value);

      pointersPage.set('prefs.settings.touchpad.sensitivity2.value', 2);
      assertEquals(2, slider.pref.value);
    });

    test('haptic touchpad', function() {
      assertTrue(
          pointersPage.shadowRoot.querySelector('#touchpadHapticFeedbackToggle')
              .checked);

      const slider = assert(pointersPage.shadowRoot.querySelector(
          '#touchpadHapticClickSensitivity'));
      assertEquals(3, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 39 /* right */, [],
          'ArrowRight');
      assertEquals(
          5, devicePage.prefs.settings.touchpad.haptic_click_sensitivity.value);

      pointersPage.set(
          'prefs.settings.touchpad.haptic_click_sensitivity.value', 1);
      assertEquals(1, slider.pref.value);
    });

    test('link doesn\'t activate control', function() {
      expectReverseScrollValue(pointersPage, false);

      // Tapping the link shouldn't enable the radio button.
      const reverseScrollLabel =
          pointersPage.shadowRoot.querySelector('#enableReverseScrollingLabel');
      const a = reverseScrollLabel.$.container.querySelector('a');
      assertTrue(!!a);
      // Prevent actually opening a link, which would block test.
      a.removeAttribute('href');
      a.click();
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking toggle changes pref.
      const reverseScrollToggle = pointersPage.shadowRoot.querySelector(
          '#enableReverseScrollingToggle');
      reverseScrollToggle.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking the row changes pref.
      const reverseScrollSettings =
          pointersPage.shadowRoot.querySelector('#reverseScrollRow');
      reverseScrollSettings.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);
    });

    test('pointing stick acceleration toggle', function() {
      const toggle = assert(
          pointersPage.shadowRoot.querySelector('#pointingStickAcceleration'));
      assertEquals(true, toggle.pref.value);
      toggle.click();
      assertEquals(
          false, devicePage.prefs.settings.pointing_stick.acceleration.value);

      pointersPage.set(
          'prefs.settings.pointing_stick.acceleration.value', true);
      assertEquals(true, toggle.pref.value);
    });

    test('pointing stick speed slider', function() {
      const slider = assert(pointersPage.shadowRoot.querySelector(
          '#pointingStick settings-slider'));
      assertEquals(4, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 37, [], 'ArrowLeft');
      assertEquals(
          3, devicePage.prefs.settings.pointing_stick.sensitivity.value);

      pointersPage.set('prefs.settings.pointing_stick.sensitivity.value', 5);
      assertEquals(5, slider.pref.value);
    });

    test('Deep link to pointing stick primary button setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '437',
          pointersPage.shadowRoot
              .querySelector('#pointingStickSwapButtonDropdown')
              .shadowRoot.querySelector('select'),
          'Pointing stick primary button dropdown');
    });

    test('Deep link to pointing stick acceleration setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '436',
          pointersPage.shadowRoot.querySelector('#pointingStickAcceleration')
              .shadowRoot.querySelector('cr-toggle'),
          'Pointing stick acceleration slider');
    });

    test('Deep link to pointing stick speed setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '435',
          pointersPage.shadowRoot.querySelector('#pointingStickSpeedSlider')
              .shadowRoot.querySelector('cr-slider'),
          'Pointing stick speed slider');
    });

    test('Deep link to touchpad speed', async () => {
      return checkDeepLink(
          routes.POINTERS, '405',
          pointersPage.shadowRoot.querySelector('#touchpadSensitivity')
              .shadowRoot.querySelector('cr-slider'),
          'Touchpad speed slider');
    });
  });

  suite(assert(TestNames.Keyboard), function() {
    const name = k => `prefs.settings.language.${k}.value`;
    const get = k => devicePage.get(name(k));
    const set = (k, v) => devicePage.set(name(k), v);
    let keyboardPage;

    setup(async () => {
      setDeviceSplitEnabled(false);
      await init();
      keyboardPage = await showAndGetDeviceSubpage('keyboard', routes.KEYBOARD);
    });

    test('keyboard', async () => {
      // Initially, the optional keys are hidden.
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));

      // Pretend no internal keyboard is available.
      const keyboardParams = {
        'showCapsLock': false,
        'showExternalMetaKey': false,
        'showAppleCommandKey': false,
        'hasLauncherKey': false,
        'hasAssistantKey': false,
      };
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertFalse(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      // Pretend a Caps Lock key is now available.
      keyboardParams['showCapsLock'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertFalse(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      // Add a non-Apple external keyboard.
      keyboardParams['showExternalMetaKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertFalse(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      // Add an Apple keyboard.
      keyboardParams['showAppleCommandKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      // Add an internal keyboard.
      keyboardParams['hasLauncherKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertFalse(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      // Pretend an Assistant key is now available.
      keyboardParams['hasAssistantKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot.querySelector('#externalCommandKey'));
      assertTrue(!!keyboardPage.shadowRoot.querySelector('#assistantKey'));

      const collapse = keyboardPage.shadowRoot.querySelector('iron-collapse');
      assertTrue(!!collapse);
      assertTrue(collapse.opened);

      assertEquals(
          500,
          keyboardPage.shadowRoot.querySelector('#delaySlider').pref.value);
      assertEquals(
          500,
          keyboardPage.shadowRoot.querySelector('#repeatRateSlider')
              .pref.value);

      // Test interaction with the settings-slider's underlying cr-slider.
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.shadowRoot.querySelector('#delaySlider')
              .shadowRoot.querySelector('cr-slider'),
          37 /* left */, [], 'ArrowLeft');
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.shadowRoot.querySelector('#repeatRateSlider')
              .shadowRoot.querySelector('cr-slider'),
          39, [], 'ArrowRight');
      await flushTasks();
      assertEquals(1000, get('xkb_auto_repeat_delay_r2'));
      assertEquals(300, get('xkb_auto_repeat_interval_r2'));

      // Test sliders change when prefs change.
      set('xkb_auto_repeat_delay_r2', 1500);
      await flushTasks();
      assertEquals(
          1500,
          keyboardPage.shadowRoot.querySelector('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 2000);
      await flushTasks();
      assertEquals(
          2000,
          keyboardPage.shadowRoot.querySelector('#repeatRateSlider')
              .pref.value);

      // Test sliders round to nearest value when prefs change.
      set('xkb_auto_repeat_delay_r2', 600);
      await flushTasks();
      assertEquals(
          500,
          keyboardPage.shadowRoot.querySelector('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 45);
      await flushTasks();
      assertEquals(
          50,
          keyboardPage.shadowRoot.querySelector('#repeatRateSlider')
              .pref.value);

      set('xkb_auto_repeat_enabled_r2', false);
      assertFalse(collapse.opened);

      // Test keyboard shortcut viewer button.
      keyboardPage.shadowRoot.querySelector('#keyboardShortcutViewer').click();
      assertEquals(
          1,
          DevicePageBrowserProxyImpl.getInstance().getCallCount(
              'showKeyboardShortcutViewer'));
    });

    test('Deep link to keyboard shortcuts', async () => {
      return checkDeepLink(
          routes.KEYBOARD, '413',
          keyboardPage.shadowRoot.querySelector('#keyboardShortcutViewer')
              .shadowRoot.querySelector('cr-icon-button'),
          'Keyboard shortcuts button');
    });
  });

  suite(assert(TestNames.GraphicsTablet), function() {
    let graphicsTabletPage;
    let inputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeGraphicsTablets(fakeGraphicsTablets);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async function() {
      setPeripheralCustomizationEnabled(true);
      await init();
      const row =
          assert(devicePage.shadowRoot.querySelector(`#main #tabletRow`));
      row.click();
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);
      const page = devicePage.shadowRoot.querySelector(
          'settings-graphics-tablet-subpage');
      assert(page);
      return Promise.resolve(page).then(function(page) {
        graphicsTabletPage = page;
      });
    });

    test('graphics tablet subpage visibility', async () => {
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);
      const items = graphicsTabletPage.shadowRoot.querySelectorAll('.device');
      // Verify that all graphics tablets are displayed and their ids are same
      // with the data in the provider.
      assertEquals(items.length, fakeGraphicsTablets.length);
      assertTrue(isVisible(items[0]));
      assertEquals(
          Number(items[0].getAttribute('data-evdev-id')),
          fakeGraphicsTablets[0].id);
      assertTrue(isVisible(items[1]));
      assertEquals(
          Number(items[1].getAttribute('data-evdev-id')),
          fakeGraphicsTablets[1].id);

      // Verify that the customize-tablet-buttons and customize-pen-buttons
      // crLinkRow are visible.
      const customizeTabletButtons =
          graphicsTabletPage.shadowRoot.querySelector(
              '#customizeTabletButtons');
      assert(customizeTabletButtons);
      assertTrue(isVisible(customizeTabletButtons));

      // Verify clicking the customize table buttons row will be redirecting
      // to the customize table buttons subpage.
      customizeTabletButtons.click();
      await flushTasks();
      assertEquals(
          routes.CUSTOMIZE_TABLET_BUTTONS, Router.getInstance().currentRoute);

      const urlSearchQuery =
          Router.getInstance().getQueryParameters().get('graphicsTabletId');
      assertTrue(!!urlSearchQuery);
      const graphicsTabletId = Number(urlSearchQuery);
      assertFalse(isNaN(graphicsTabletId));
      assertEquals(fakeGraphicsTablets[0].id, graphicsTabletId);

      // Verify clicking the customize pen buttons row will be redirected
      // to the customize table buttons subpage.
      Router.getInstance().navigateTo(routes.GRAPHICS_TABLET);
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);
      const customizePenButtons =
          graphicsTabletPage.shadowRoot.querySelector('#customizePenButtons');

      assertTrue(isVisible(customizePenButtons));
      customizePenButtons.click();
      await flushTasks();
      assertEquals(
          routes.CUSTOMIZE_PEN_BUTTONS, Router.getInstance().currentRoute);
      const graphicsTabletPenId = Number(
          Router.getInstance().getQueryParameters().get('graphicsTabletId'));
      assertFalse(isNaN(graphicsTabletPenId));
      assertEquals(fakeGraphicsTablets[0].id, graphicsTabletPenId);
    });
  });

  suite(assert(TestNames.Power), function() {
    /**
     * Sets power sources using a deep copy of |sources|.
     * @param {Array<PowerSource>} sources
     * @param {string} powerSourceId
     * @param {bool} isLowPowerCharger
     */
    function setPowerSources(
        sources, powerSourceId, isExternalPowerUSB, isExternalPowerAC) {
      const sourcesCopy = sources.map(function(source) {
        return Object.assign({}, source);
      });
      webUIListenerCallback(
          'power-sources-changed', sourcesCopy, powerSourceId,
          isExternalPowerUSB, isExternalPowerAC);
    }

    suite('power settings', function() {
      let powerPage;
      let powerSourceRow;
      let powerSourceSelect;
      let acIdleSelect;
      let lidClosedToggle;
      let adaptiveChargingToggle;
      let batterySaverToggle;

      suiteSetup(function() {
        // Adaptive charging setting should be shown.
        loadTimeData.overrideValues({
          isAdaptiveChargingEnabled: true,
        });
      });

      setup(async function() {
        await init();
        return showAndGetDeviceSubpage('power', routes.POWER)
            .then(function(page) {
              powerPage = page;
              powerSourceRow =
                  assert(powerPage.shadowRoot.querySelector('#powerSourceRow'));
              powerSourceSelect =
                  assert(powerPage.shadowRoot.querySelector('#powerSource'));
              assertEquals(
                  1,
                  DevicePageBrowserProxyImpl.getInstance().getCallCount(
                      'updatePowerStatus'));

              lidClosedToggle = assert(
                  powerPage.shadowRoot.querySelector('#lidClosedToggle'));
              adaptiveChargingToggle =
                  assert(powerPage.shadowRoot.querySelector(
                      '#adaptiveChargingToggle'));
              batterySaverToggle = assert(
                  powerPage.shadowRoot.querySelector('#batterySaverToggle'));

              assertEquals(
                  1,
                  DevicePageBrowserProxyImpl.getInstance().getCallCount(
                      'requestPowerManagementSettings'));
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                  ],
                  IdleBehavior.DISPLAY_OFF_SLEEP,
                  IdleBehavior.DISPLAY_OFF_SLEEP, false /* acIdleManaged */,
                  false /* batteryIdleManaged */, LidClosedBehavior.SUSPEND,
                  false /* lidClosedControlled */, true /* hasLid */,
                  false /* adaptiveCharging */,
                  false /* adaptiveChargingManaged */,
                  true /* batterySaverFeatureEnabled */);
            });
      });

      test('no battery', async function() {
        await init();
        const batteryStatus = {
          present: false,
          charging: false,
          calculating: false,
          percent: -1,
          statusText: '',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        flush();

        // Power source row is hidden since there's no battery.
        assertTrue(powerSourceRow.hidden);
        // Battery Saver is also hidden.
        assertTrue(batterySaverToggle.hidden);
        // Idle settings while on battery and while charging should not be
        // visible if the battery is not present.
        assertEquals(
            null, powerPage.shadowRoot.querySelector('#batteryIdleSettingBox'));
        assertEquals(
            null, powerPage.shadowRoot.querySelector('#acIdleSettingBox'));

        const acIdleSelect = assert(
            powerPage.shadowRoot.querySelector('#noBatteryAcIdleSelect'));
        // Expect the "When idle" dropdown options to appear instead.
        assert(acIdleSelect);

        // Select a "When idle" selection and expect it to be set.
        selectValue(acIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(
            IdleBehavior.DISPLAY_ON,
            DevicePageBrowserProxyImpl.getInstance().acIdleBehavior);
      });

      test('power sources', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false, false);
        flush();

        // Power sources row is visible but dropdown is hidden.
        assertFalse(powerSourceRow.hidden);
        assertTrue(powerSourceSelect.hidden);

        // Attach a dual-role USB device.
        const powerSource = {
          id: '2',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false, false);
        flush();

        // "Battery" should be selected.
        assertFalse(powerSourceSelect.hidden);
        assertEquals('', powerSourceSelect.value);

        // Select the power source.
        setPowerSources([powerSource], powerSource.id, true, false);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);

        // Send another power source; the first should still be selected.
        const otherPowerSource = Object.assign({}, powerSource);
        otherPowerSource.id = '3';
        setPowerSources(
            [otherPowerSource, powerSource], powerSource.id, true, false);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);
      });

      test('choose power source', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));

        // Attach a dual-role USB device.
        const powerSource = {
          id: '3',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false, false);
        flush();

        // Select the device.
        selectValue(powerSourceSelect, powerSourceSelect.children[1].value);
        assertEquals(
            powerSource.id,
            DevicePageBrowserProxyImpl.getInstance().powerSourceId);
      });

      test('set AC idle behavior', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false, false);
        flush();

        acIdleSelect =
            assert(powerPage.shadowRoot.querySelector('#acIdleSelect'));
        selectValue(acIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(
            IdleBehavior.DISPLAY_ON,
            DevicePageBrowserProxyImpl.getInstance().acIdleBehavior);
      });

      test('set battery idle behavior', function() {
        return new Promise(function(resolve) {
                 // Indicate battery presence so that idle settings box while
                 // on battery is visible.
                 const batteryStatus = {
                   present: true,
                   charging: false,
                   calculating: false,
                   percent: 50,
                   statusText: '5 hours left',
                 };
                 webUIListenerCallback(
                     'battery-status-changed',
                     Object.assign({}, batteryStatus));
                 microTask.run(resolve);
               })
            .then(function() {
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              selectValue(batteryIdleSelect, IdleBehavior.DISPLAY_ON);
              assertEquals(
                  IdleBehavior.DISPLAY_ON,
                  DevicePageBrowserProxyImpl.getInstance().batteryIdleBehavior);
            });
      });

      test('set lid behavior', function() {
        const sendLid = function(lidBehavior) {
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
              false /* acIdleManaged */, false /* batteryIdleManaged */,
              lidBehavior, false /* lidClosedControlled */, true /* hasLid */,
              false /* adaptiveCharging */, false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
        };

        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);

        lidClosedToggle.shadowRoot.querySelector('#control').click();
        assertEquals(
            LidClosedBehavior.DO_NOTHING,
            DevicePageBrowserProxyImpl.getInstance().lidClosedBehavior);
        sendLid(LidClosedBehavior.DO_NOTHING);
        assertFalse(lidClosedToggle.checked);

        lidClosedToggle.shadowRoot.querySelector('#control').click();
        assertEquals(
            LidClosedBehavior.SUSPEND,
            DevicePageBrowserProxyImpl.getInstance().lidClosedBehavior);
        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);
      });
      test('display idle behavior for shut_down/stop_session', function() {
        return new Promise(function(resolve) {
                 // Send power management settings first.
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                       IdleBehavior.SHUT_DOWN,
                       IdleBehavior.STOP_SESSION,
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                       IdleBehavior.SHUT_DOWN,
                       IdleBehavior.STOP_SESSION,
                     ],
                     IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                     true /* acIdleManaged */, true /* batteryIdleManaged */,
                     LidClosedBehavior.DO_NOTHING,
                     false /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */,
                     false /* adaptiveChargingManaged */,
                     true /* batterySaverFeatureEnabled */);
                 microTask.run(resolve);
               })
            .then(function() {
              // Indicate battery presence so that battery idle settings
              // box becomes visible. Default option should be selected
              // properly even when battery idle settings box is stamped
              // later.
              const batteryStatus = {
                present: true,
                charging: false,
                calculating: false,
                percent: 50,
                statusText: '5 hours left',
              };
              webUIListenerCallback(
                  'battery-status-changed', Object.assign({}, batteryStatus));
              return new Promise(function(resolve) {
                microTask.run(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertFalse(batteryIdleSelect.disabled);
              const acIdleSelect =
                  assert(powerPage.shadowRoot.querySelector('#acIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                    IdleBehavior.SHUT_DOWN,
                    IdleBehavior.STOP_SESSION,
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                    IdleBehavior.SHUT_DOWN,
                    IdleBehavior.STOP_SESSION,
                  ],
                  IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                  true /* acIdleManaged */, true /* batteryIdleManaged */,
                  LidClosedBehavior.DO_NOTHING, false /* lidClosedControlled */,
                  true /* hasLid */, false /* adaptiveCharging */,
                  false /* adaptiveChargingManaged */,
                  true /* batterySaverFeatureEnabled */);
              return new Promise(function(resolve) {
                microTask.run(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertFalse(batteryIdleSelect.disabled);
              const acIdleSelect =
                  assert(powerPage.shadowRoot.querySelector('#acIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            });
      });
      test('display idle and lid behavior', function() {
        return new Promise(function(resolve) {
                 // Send power management settings first.
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                     ],
                     IdleBehavior.DISPLAY_ON, IdleBehavior.DISPLAY_OFF,
                     false /* acIdleManaged */, false /* batteryIdleManaged */,
                     LidClosedBehavior.DO_NOTHING,
                     false /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */,
                     false /* adaptiveChargingManaged */,
                     true /* batterySaverFeatureEnabled */);
                 microTask.run(resolve);
               })
            .then(function() {
              // Indicate battery presence so that battery idle settings
              // box becomes visible. Default option should be selected
              // properly even when battery idle settings box is stamped
              // later.
              const batteryStatus = {
                present: true,
                charging: false,
                calculating: false,
                percent: 50,
                statusText: '5 hours left',
              };
              webUIListenerCallback(
                  'battery-status-changed', Object.assign({}, batteryStatus));
              return new Promise(function(resolve) {
                microTask.run(resolve);
              });
            })
            .then(function() {
              acIdleSelect =
                  assert(powerPage.shadowRoot.querySelector('#acIdleSelect'));
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_ON.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#acIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                  ],
                  IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_ON,
                  false /* acIdleManaged */, false /* batteryIdleManaged */,
                  LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
                  true /* hasLid */, false /* adaptiveCharging */,
                  false /* adaptiveChargingManaged */,
                  true /* batterySaverFeatureEnabled */);
              return new Promise(function(resolve) {
                microTask.run(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_ON.toString(), batteryIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertFalse(batteryIdleSelect.disabled);
              assertEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#acIdleManagedIndicator'));
              assertEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            });
      });

      test('display managed idle and lid behavior', function() {
        // When settings are managed, the controls should be disabled and
        // the indicators should be shown.
        return new Promise(function(resolve) {
                 // Indicate battery presence so that idle settings box while
                 // on battery is visible.
                 const batteryStatus = {
                   present: true,
                   charging: false,
                   calculating: false,
                   percent: 50,
                   statusText: '5 hours left',
                 };
                 webUIListenerCallback(
                     'battery-status-changed',
                     Object.assign({}, batteryStatus));
                 sendPowerManagementSettings(
                     [IdleBehavior.SHUT_DOWN], [IdleBehavior.SHUT_DOWN],
                     IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                     true /* acIdleManaged */, true /* batteryIdleManaged */,
                     LidClosedBehavior.SHUT_DOWN,
                     true /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */,
                     false /* adaptiveChargingManaged */,
                     true /* batterySaverFeatureEnabled */);
                 microTask.run(resolve);
               })
            .then(function() {
              acIdleSelect =
                  assert(powerPage.shadowRoot.querySelector('#acIdleSelect'));
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertTrue(acIdleSelect.disabled);
              assertTrue(batteryIdleSelect.disabled);
              assertNotEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#acIdleManagedIndicator'));
              assertNotEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidShutDownLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertTrue(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [IdleBehavior.DISPLAY_OFF], [IdleBehavior.DISPLAY_OFF],
                  IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
                  false /* acIdleManaged */, false /* batteryIdleManaged */,
                  LidClosedBehavior.STOP_SESSION,
                  true /* lidClosedControlled */, true /* hasLid */,
                  false /* adaptiveCharging */,
                  false /* adaptiveChargingManaged */,
                  true /* batterySaverFeatureEnabled */);
              return new Promise(function(resolve) {
                microTask.run(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect = assert(
                  powerPage.shadowRoot.querySelector('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
              assertTrue(acIdleSelect.disabled);
              assertTrue(batteryIdleSelect.disabled);
              assertEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#acIdleManagedIndicator'));
              assertEquals(
                  null,
                  powerPage.shadowRoot.querySelector(
                      '#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSignOutLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertTrue(lidClosedToggle.isPrefEnforced());
            });
      });

      test('hide lid behavior when lid not present', function() {
        return new Promise(function(resolve) {
                 assertFalse(
                     powerPage.shadowRoot.querySelector('#lidClosedToggle')
                         .hidden);
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                     ],
                     IdleBehavior.DISPLAY_OFF_SLEEP,
                     IdleBehavior.DISPLAY_OFF_SLEEP, false /* acIdleManaged */,
                     false /* batteryIdleManaged */, LidClosedBehavior.SUSPEND,
                     false /* lidClosedControlled */, false /* hasLid */,
                     false /* adaptiveCharging */,
                     false /* adaptiveChargingManaged */,
                     true /* batterySaverFeatureEnabled */);
                 microTask.run(resolve);
               })
            .then(function() {
              assertTrue(powerPage.shadowRoot.querySelector('#lidClosedToggle')
                             .hidden);
            });
      });

      test(
          'hide display controlled battery idle behavior when battery not present',
          function() {
            return new Promise(function(resolve) {
                     const batteryStatus = {
                       present: false,
                       charging: false,
                       calculating: false,
                       percent: -1,
                       statusText: '',
                     };
                     webUIListenerCallback(
                         'battery-status-changed',
                         Object.assign({}, batteryStatus));
                     flush();
                     microTask.run(resolve);
                   })
                .then(function() {
                  assertEquals(
                      null,
                      powerPage.shadowRoot.querySelector(
                          '#batteryIdleSettingBox'));
                });
          });

      test('Deep link to sleep when laptop lid closed', async () => {
        return checkDeepLink(
            routes.POWER, '424',
            lidClosedToggle.shadowRoot.querySelector('cr-toggle'),
            'Sleep when closed toggle');
      });

      test('Adaptive charging controlled by policy', async () => {
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, true /* adaptiveCharging */,
            true /* adaptiveCharingManaged */,
            true /* batterySaverFeatureEnabled */);

        assertTrue(adaptiveChargingToggle.shadowRoot.querySelector('cr-toggle')
                       .checked);

        // Must have policy icon.
        assertTrue(isVisible(adaptiveChargingToggle.shadowRoot.querySelector(
            'cr-policy-pref-indicator')));

        // Must have toggle locked.
        assertTrue(adaptiveChargingToggle.shadowRoot.querySelector('cr-toggle')
                       .disabled);
      });

      test('Deep link to adaptive charging', async () => {
        return checkDeepLink(
            routes.POWER, '440',
            adaptiveChargingToggle.shadowRoot.querySelector('cr-toggle'),
            'Adaptive charging toggle');
      });

      test('Battery Saver hidden when feature disabled', () => {
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            false /* batterySaverFeatureEnabled */);

        assertTrue(batterySaverToggle.hidden);
      });

      test('Battery Saver toggleable', () => {
        // Battery is present.
        webUIListenerCallback('battery-status-changed', {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        });
        // There are no power sources.
        setPowerSources([], '', false, false);
        // Battery saver feature is enabled.
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);

        // Battery saver should be visible and toggleable.
        assertFalse(batterySaverToggle.hidden);
        assertFalse(batterySaverToggle.disabled);

        // Connect a dedicated AC power adapter.
        const mainsPowerSource = {
          id: '1',
          is_dedicated_charger: true,
          description: 'USB-C device',
        };
        setPowerSources([mainsPowerSource], '1', false, true);

        // Battery saver should be visible but not toggleable.
        assertFalse(batterySaverToggle.hidden);
        assertTrue(batterySaverToggle.disabled);
      });

      test('Battery Saver updates when pref updates', () => {
        function setPref(value) {
          const newPrefs = getFakePrefs();
          newPrefs.power.cros_battery_saver_active.value = value;
          powerPage.prefs = newPrefs;
          flush();
        }

        setPref(true);
        assertTrue(batterySaverToggle.checked);

        setPref(false);
        assertFalse(batterySaverToggle.checked);
      });
    });
  });

  suite(assert(TestNames.Stylus), function() {
    let stylusPage;
    let appSelector;
    let browserProxy;
    let noAppsDiv;
    let waitingDiv;

    // Shorthand for NoteAppLockScreenSupport.
    let LockScreenSupport;

    suiteSetup(function() {
      // Always show stylus settings.
      loadTimeData.overrideValues({
        hasInternalStylus: true,
      });
    });

    setup(async function() {
      await init();
      return showAndGetDeviceSubpage('stylus', routes.STYLUS)
          .then(function(page) {
            stylusPage = page;
            browserProxy = DevicePageBrowserProxyImpl.getInstance();
            appSelector = assert(page.shadowRoot.querySelector('#selectApp'));
            noAppsDiv = assert(page.shadowRoot.querySelector('#no-apps'));
            waitingDiv = assert(page.shadowRoot.querySelector('#waiting'));
            LockScreenSupport = NoteAppLockScreenSupport;

            assertEquals(1, browserProxy.getCallCount('requestNoteTakingApps'));
            assert(browserProxy.onNoteTakingAppsUpdated_);
          });
    });

    // Helper function to allocate a note app entry.
    function entry(name, value, preferred, lockScreenSupport) {
      return {
        name: name,
        value: value,
        preferred: preferred,
        lockScreenSupport: lockScreenSupport,
      };
    }

    /**  @return {?Element} */
    function noteTakingAppLockScreenSettings() {
      return stylusPage.shadowRoot.querySelector(
          '#note-taking-app-lock-screen-settings');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenToggle() {
      return stylusPage.shadowRoot.querySelector(
          '#enable-app-on-lock-screen-toggle');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenPolicyIndicator() {
      return stylusPage.shadowRoot.querySelector(
          '#enable-app-on-lock-screen-policy-indicator');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenToggleLabel() {
      return stylusPage.shadowRoot.querySelector('#lock-screen-toggle-label');
    }

    /** @return {?Element} */
    function keepLastNoteOnLockScreenToggle() {
      return stylusPage.shadowRoot.querySelector(
          '#keep-last-note-on-lock-screen-toggle');
    }

    test('stylus tools prefs', function() {
      // Both stylus tools prefs are initially false.
      assertFalse(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(
          devicePage.prefs.settings.launch_palette_on_eject_event.value);

      // Since both prefs are initially false, the launch palette on eject pref
      // toggle is disabled.
      assertTrue(isVisible(
          stylusPage.shadowRoot.querySelector('#enableStylusToolsToggle')));
      assertTrue(isVisible(stylusPage.shadowRoot.querySelector(
          '#launchPaletteOnEjectEventToggle')));
      assertTrue(stylusPage.shadowRoot
                     .querySelector('#launchPaletteOnEjectEventToggle')
                     .disabled);
      assertFalse(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(
          devicePage.prefs.settings.launch_palette_on_eject_event.value);

      // Tapping the enable stylus tools pref causes the launch palette on
      // eject pref toggle to not be disabled anymore.
      stylusPage.shadowRoot.querySelector('#enableStylusToolsToggle').click();
      assertTrue(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(stylusPage.shadowRoot
                      .querySelector('#launchPaletteOnEjectEventToggle')
                      .disabled);
      stylusPage.shadowRoot.querySelector('#launchPaletteOnEjectEventToggle')
          .click();
      assertTrue(devicePage.prefs.settings.launch_palette_on_eject_event.value);
    });

    test('choose first app if no preferred ones', function() {
      // Selector chooses the first value in list if there is no preferred
      // value set.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED),
      ]);
      flush();
      assertEquals('v1', appSelector.value);
    });

    test('choose prefered app if exists', function() {
      // Selector chooses the preferred value if set.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
      ]);
      flush();
      assertEquals('v2', appSelector.value);
    });

    test('change preferred app', function() {
      // Load app list.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
      ]);
      flush();
      assertEquals(0, browserProxy.getCallCount('setPreferredNoteTakingApp'));
      assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

      // Update select element to new value, verify browser proxy is called.
      appSelector.value = 'v1';
      stylusPage.onSelectedAppChanged_();
      assertEquals(1, browserProxy.getCallCount('setPreferredNoteTakingApp'));
      assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());
    });

    test('preferred app does not change without interaction', function() {
      // Pass various types of data to page, verify the preferred note-app
      // does not change.
      browserProxy.setNoteTakingApps([]);
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.onNoteTakingAppsUpdated_([], true);
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.addNoteTakingApp(
          entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
      ]);
      flush();
      assertEquals(0, browserProxy.getCallCount('setPreferredNoteTakingApp'));
      assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
    });

    test('Deep link to preferred app', async () => {
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED),
      ]);
      browserProxy.setAndroidAppsReceived(true);

      return checkDeepLink(
          routes.STYLUS, '417',
          stylusPage.shadowRoot.querySelector('#selectApp'),
          'Note-taking apps dropdown');
    });

    test('app-visibility', function() {
      // No apps available.
      browserProxy.setNoteTakingApps([]);
      assert(noAppsDiv.hidden);
      assert(!waitingDiv.hidden);
      assert(appSelector.hidden);

      // Waiting for apps to finish loading.
      browserProxy.setAndroidAppsReceived(true);
      assert(!noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(appSelector.hidden);

      // Apps loaded, show selector.
      browserProxy.addNoteTakingApp(
          entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
      assert(noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(!appSelector.hidden);

      // Waiting for Android apps again.
      browserProxy.setAndroidAppsReceived(false);
      assert(noAppsDiv.hidden);
      assert(!waitingDiv.hidden);
      assert(appSelector.hidden);

      browserProxy.setAndroidAppsReceived(true);
      assert(noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(!appSelector.hidden);
    });

    test('enabled-on-lock-screen', function() {
      assertFalse(isVisible(noteTakingAppLockScreenSettings()));
      assertFalse(isVisible(enableAppOnLockScreenToggle()));
      assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

      return new Promise(function(resolve) {
               // No apps available.
               browserProxy.setNoteTakingApps([]);
               microTask.run(resolve);
             })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Single app which does not support lock screen note taking.
            browserProxy.addNoteTakingApp(
                entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED));
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Add an app with lock screen support, but do not select it yet.
            browserProxy.addNoteTakingApp(
                entry('n2', 'v2', false, LockScreenSupport.SUPPORTED));
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Select the app with lock screen app support.
            appSelector.value = 'v2';
            stylusPage.onSelectedAppChanged_();
            assertEquals(
                1, browserProxy.getCallCount('setPreferredNoteTakingApp'));
            assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);

            // Preferred app updated to be enabled on lock screen.
            browserProxy.setNoteTakingApps([
              entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
              entry('n2', 'v2', true, LockScreenSupport.ENABLED),
            ]);
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertTrue(enableAppOnLockScreenToggle().checked);

            // Select the app that does not support lock screen again.
            appSelector.value = 'v1';
            stylusPage.onSelectedAppChanged_();
            assertEquals(
                2, browserProxy.getCallCount('setPreferredNoteTakingApp'));
            assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
          });
    });

    test('initial-app-lock-screen-enabled', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               microTask.run(resolve);
             })
          .then(function() {
            flush();

            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps(
                [entry('n1', 'v1', true, LockScreenSupport.ENABLED)]);

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps(
                [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps([]);
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
          });
    });

    test('tap-on-enable-note-taking-on-lock-screen', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               microTask.run(resolve);
             })
          .then(function() {
            flush();

            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            enableAppOnLockScreenToggle().click();
            assertEquals(
                1,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.ENABLED,
                browserProxy.getPreferredAppLockScreenState());

            enableAppOnLockScreenToggle().click();
            assertEquals(
                2,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertEquals(
                LockScreenSupport.SUPPORTED,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('tap-on-enable-note-taking-on-lock-screen-label', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               microTask.run(resolve);
             })
          .then(function() {
            flush();

            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);

            enableAppOnLockScreenToggleLabel().click();
            assertEquals(
                1,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.ENABLED,
                browserProxy.getPreferredAppLockScreenState());

            enableAppOnLockScreenToggleLabel().click();
            assertEquals(
                2,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertEquals(
                LockScreenSupport.SUPPORTED,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('lock-screen-apps-disabled-by-policy', function() {
      assertFalse(isVisible(enableAppOnLockScreenToggle()));
      assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

      return new Promise(function(resolve) {
               // Add an app with lock screen support.
               browserProxy.addNoteTakingApp(entry(
                   'n2', 'v2', true, LockScreenSupport.NOT_ALLOWED_BY_POLICY));
               microTask.run(resolve);
             })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // The toggle should be disabled, so enabling app on lock screen
            // should not be attempted.
            enableAppOnLockScreenToggle().click();
            assertEquals(
                0,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();

            // Tap on label should not work either.
            enableAppOnLockScreenToggleLabel().click();
            assertEquals(
                0,
                browserProxy.getCallCount(
                    'setPreferredNoteTakingAppEnabledOnLockScreen'));

            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.NOT_ALLOWED_BY_POLICY,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('keep-last-note-on-lock-screen', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps([
                 entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED),
                 entry('n2', 'v2', false, LockScreenSupport.SUPPORTED),
               ]);
               microTask.run(resolve);
             })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));

            browserProxy.setNoteTakingApps([
              entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
              entry('n2', 'v2', true, LockScreenSupport.SUPPORTED),
            ]);
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));

            browserProxy.setNoteTakingApps([
              entry('n2', 'v2', true, LockScreenSupport.ENABLED),
            ]);
            return new Promise(function(resolve) {
              microTask.run(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(keepLastNoteOnLockScreenToggle()));
            assertTrue(keepLastNoteOnLockScreenToggle().checked);

            // Clicking the toggle updates the pref value.
            keepLastNoteOnLockScreenToggle()
                .shadowRoot.querySelector('#control')
                .click();
            assertFalse(keepLastNoteOnLockScreenToggle().checked);

            assertFalse(
                devicePage.prefs.settings.restore_last_lock_screen_note.value);

            // Changing the pref value updates the toggle.
            devicePage.set(
                'prefs.settings.restore_last_lock_screen_note.value', true);
            assertTrue(keepLastNoteOnLockScreenToggle().checked);
          });
    });

    test(
        'Clicking "Find more stylus apps" button should open Google Play',
        async () => {
          const findMoreAppsLink =
              stylusPage.shadowRoot.querySelector('#findMoreAppsLink');
          assertTrue(
              !!findMoreAppsLink, 'Find more apps link element does not exist');
          assertFalse(findMoreAppsLink.hidden, 'Find more apps link is hidden');

          findMoreAppsLink.click();
          const url = await browserProxy.whenCalled('showPlayStore');
          const expectedUrl =
              'https://play.google.com/store/apps/collection/promotion_30023cb_stylus_apps';
          assertEquals(expectedUrl, url);
        });
  });

  suite(assert(TestNames.Storage), function() {
    /** @type {!Element} */
    let storagePage;

    /**
     * Simulate storage size stat callback.
     * @param {string} availableSize
     * @param {string} usedSize
     * @param {number} usedRatio
     * @param {number} spaceState
     */
    function sendStorageSizeStat(
        usedSize, availableSize, usedRatio, spaceState) {
      webUIListenerCallback('storage-size-stat-changed', {
        usedSize: usedSize,
        availableSize: availableSize,
        usedRatio: usedRatio,
        spaceState: spaceState,
      });
      flush();
    }

    /**
     * @param {string} id
     * @return {string}
     */
    function getStorageItemLabelFromId(id) {
      const rowItem = storagePage.shadowRoot.querySelector('#' + id).shadowRoot;
      return rowItem.querySelector('#label').innerText;
    }

    /**
     * @param {string} id
     * @return {string}
     */
    function getStorageItemSubLabelFromId(id) {
      const rowItem = storagePage.shadowRoot.querySelector('#' + id).shadowRoot;
      return rowItem.querySelector('#subLabel').innerText;
    }

    async function setupPage() {
      PolymerTest.clearBody();
      await init();
      storagePage = await showAndGetDeviceSubpage('storage', routes.STORAGE);
      storagePage.stopPeriodicUpdate_();
    }

    suiteSetup(() => {
      // Disable animations so sub-pages open within one event loop.
      testing.Test.disableAnimationsAndTransitions();
    });

    setup(async () => {
      await setupPage();
    });

    test('storage stats size', async function() {
      // Low available storage space.
      sendStorageSizeStat('9.1 GB', '0.9 GB', 0.91, StorageSpaceState.LOW);
      assertEquals('91%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('9%', storagePage.$.availableLabelArea.style.width);
      assertTrue(
          isVisible(storagePage.shadowRoot.querySelector('#lowMessage')));
      assertFalse(isVisible(
          storagePage.shadowRoot.querySelector('#criticallyLowMessage')));
      assertTrue(!!storagePage.shadowRoot.querySelector('#bar.space-low'));
      assertFalse(
          !!storagePage.shadowRoot.querySelector('#bar.space-critically-low'));
      assertEquals(
          '9.1 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '0.9 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);

      // Critically low available storage space.
      sendStorageSizeStat(
          '9.7 GB', '0.3 GB', 0.97, StorageSpaceState.CRITICALLY_LOW);
      assertEquals('97%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('3%', storagePage.$.availableLabelArea.style.width);
      assertFalse(
          isVisible(storagePage.shadowRoot.querySelector('#lowMessage')));
      assertTrue(isVisible(
          storagePage.shadowRoot.querySelector('#criticallyLowMessage')));
      assertFalse(!!storagePage.shadowRoot.querySelector('#bar.space-low'));
      assertTrue(
          !!storagePage.shadowRoot.querySelector('#bar.space-critically-low'));
      assertEquals(
          '9.7 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '0.3 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);

      // Normal storage usage.
      sendStorageSizeStat('2.5 GB', '7.5 GB', 0.25, StorageSpaceState.NORMAL);
      assertEquals('25%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('75%', storagePage.$.availableLabelArea.style.width);
      assertFalse(
          isVisible(storagePage.shadowRoot.querySelector('#lowMessage')));
      assertFalse(isVisible(
          storagePage.shadowRoot.querySelector('#criticallyLowMessage')));
      assertFalse(!!storagePage.shadowRoot.querySelector('#bar.space-low'));
      assertFalse(
          !!storagePage.shadowRoot.querySelector('#bar.space-critically-low'));
      assertEquals(
          '2.5 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '7.5 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);
    });

    test('system size', async function() {
      assertEquals(
          'System',
          storagePage.shadowRoot.querySelector('#systemSizeLabel').innerText);
      assertEquals(
          'Calculating…',
          storagePage.shadowRoot.querySelector('#systemSizeSubLabel')
              .innerText);

      // Send system size callback.
      webUIListenerCallback('storage-system-size-changed', '8.4 GB');
      flush();
      assertEquals(
          '8.4 GB',
          storagePage.shadowRoot.querySelector('#systemSizeSubLabel')
              .innerText);

      // In guest mode, the system row should be hidden.
      storagePage.isEphemeralUser_ = true;
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot.querySelector('#systemSize')));
    });

    test('apps extensions size', async function() {
      assertEquals(
          'Apps and extensions', getStorageItemLabelFromId('appsSize'));
      assertEquals('Calculating…', getStorageItemSubLabelFromId('appsSize'));

      // Send apps size callback.
      webUIListenerCallback('storage-apps-size-changed', '59.5 KB');
      flush();
      assertEquals('59.5 KB', getStorageItemSubLabelFromId('appsSize'));
    });

    test('other users size', async function() {
      // The other users row is visible by default, displaying
      // "calculating...".
      assertTrue(
          isVisible(storagePage.shadowRoot.querySelector('#otherUsersSize')));
      assertEquals('Other users', getStorageItemLabelFromId('otherUsersSize'));
      assertEquals(
          'Calculating…', getStorageItemSubLabelFromId('otherUsersSize'));

      // Simulate absence of other users.
      webUIListenerCallback('storage-other-users-size-changed', '0 B', true);
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot.querySelector('#otherUsersSize')));

      // Send other users callback with a size that is not null.
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertTrue(
          isVisible(storagePage.shadowRoot.querySelector('#otherUsersSize')));
      assertEquals('322 MB', getStorageItemSubLabelFromId('otherUsersSize'));

      // If the user is in Guest mode, the row is not visible.
      storagePage.isEphemeralUser_ = true;
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot.querySelector('#otherUsersSize')));
    });

    test('drive offline size', async () => {
      async function assertDriveOfflineSizeVisibility(params) {
        loadTimeData.overrideValues({
          enableDriveFsBulkPinning: params.enableDriveFsBulkPinning,
          showGoogleDriveSettingsPage: params.showGoogleDriveSettingsPage,
        });
        await setupPage();
        devicePage.set('prefs.gdata.disabled.value', !params.isDriveEnabled);
        await flushTasks();
        const expectedState =
            (params.isVisible) ? 'be visible' : 'not be visible';
        assertEquals(
            params.isVisible,
            isVisible(
                storagePage.shadowRoot.getElementById('driveOfflineSize')),
            `Expected #driveOfflineSize to ${expectedState} with params: ${
                JSON.stringify(params)}`);
      }

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: true,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: true,
        isVisible: true,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: true,
        isVisible: true,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: true,
        isVisible: true,
      });
    });
  });

  suite('When OsSettingsRevampWayfinding feature is enabled', () => {
    setup(() => {
      loadTimeData.overrideValues({isRevampWayfindingEnabled: true});
    });

    test('Power row is not visible', async () => {
      await init();
      const powerRow = devicePage.shadowRoot.getElementById('powerRow');
      assertFalse(isVisible(powerRow));
    });

    test('Storage row is not visible', async () => {
      await init();
      const storageRow = devicePage.shadowRoot.getElementById('storageRow');
      assertFalse(isVisible(storageRow));
    });

    test('Printing settings card is visible', async () => {
      await init();
      const printingSettingsCard =
          devicePage.shadowRoot.querySelector('printing-settings-card');
      assertTrue(isVisible(printingSettingsCard));
    });
  });
});
