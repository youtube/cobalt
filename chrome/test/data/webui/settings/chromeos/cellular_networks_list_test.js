// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeatureState} from 'chrome://os-settings/os_settings.js';
import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestMultideviceBrowserProxy} from './multidevice_page/test_multidevice_browser_proxy.js';

suite('CellularNetworksList', function() {
  let cellularNetworkList;

  /** @type {!CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  let eSimManagerRemote;
  let browserProxy;

  setup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  teardown(function() {
    cellularNetworkList.remove();
    cellularNetworkList = null;
  });

  function init() {
    cellularNetworkList = document.createElement('cellular-networks-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    cellularNetworkList.style.height = '100%';
    cellularNetworkList.style.width = '100%';
    document.body.appendChild(cellularNetworkList);
    flush();
  }

  function setManagedPropertiesForTest(type, properties) {
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(type, true);
    const networks = [];
    for (let i = 0; i < properties.length; i++) {
      mojoApi_.setManagedPropertiesForTest(properties[i]);
      networks.push(OncMojo.managedPropertiesToNetworkState(properties[i]));
    }
    cellularNetworkList.cellularDeviceState =
        mojoApi_.getDeviceStateForTest(type);
    cellularNetworkList.networks = networks;
  }

  function initSimInfos() {
    let deviceState = cellularNetworkList.cellularDeviceState;
    if (!deviceState) {
      deviceState = {
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
      };
    }
    if (!deviceState.simInfos) {
      deviceState.simInfos = [];
    }
    cellularNetworkList.cellularDeviceState = deviceState;
  }

  function setIsSmdsSupportEnabled(value) {
    cellularNetworkList.smdsSupportEnabled_ = value;
  }

  function addPSimSlot() {
    initSimInfos();
    // Make a copy so observers get fired.
    const simInfos = [...cellularNetworkList.cellularDeviceState.simInfos];
    simInfos.push({
      iccid: '',
    });
    cellularNetworkList.set('cellularDeviceState.simInfos', simInfos);
    return flushAsync();
  }

  function addESimSlot() {
    initSimInfos();
    // Make a copy so observers get fired.
    const simInfos = [...cellularNetworkList.cellularDeviceState.simInfos];
    simInfos.push({
      eid: 'eid',
    });
    cellularNetworkList.set('cellularDeviceState.simInfos', simInfos);
    return flushAsync();
  }

  function clearSimSlots() {
    cellularNetworkList.set('cellularDeviceState.simInfos', []);
    return flushAsync();
  }

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Tether, cellular and eSIM profiles', async () => {
    eSimManagerRemote.addEuiccForTest(2);
    init();
    browserProxy.setInstantTetheringStateForTest(
        MultiDeviceFeatureState.ENABLED_BY_USER);

    const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_esim1');
    eSimNetwork1.typeProperties.cellular.eid =
        '11111111111111111111111111111111';
    const eSimNetwork2 = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_esim2');
    eSimNetwork2.typeProperties.cellular.eid =
        '22222222222222222222222222222222';
    setManagedPropertiesForTest(NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular2'),
      eSimNetwork1,
      eSimNetwork2,
      OncMojo.getDefaultManagedProperties(NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultManagedProperties(NetworkType.kTether, 'tether2'),
    ]);
    addPSimSlot();
    addESimSlot();

    await flushAsync();

    const eSimNetworkList =
        cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');
    assertTrue(!!eSimNetworkList);

    const pSimNetworkList =
        cellularNetworkList.shadowRoot.querySelector('#psimNetworkList');
    assertTrue(!!pSimNetworkList);

    const tetherNetworkList =
        cellularNetworkList.shadowRoot.querySelector('#tetherNetworkList');
    assertTrue(!!tetherNetworkList);

    assertEquals(2, eSimNetworkList.networks.length);
    assertEquals(2, pSimNetworkList.networks.length);
    assertEquals(2, tetherNetworkList.networks.length);
    assertEquals(2, eSimNetworkList.customItems.length);
  });

  test(
      'Fire show cellular setup event on eSim no network link click',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        init();
        addESimSlot();
        await flushAsync();
        const noEsimNetworksMessageWithLinkAnchor =
            cellularNetworkList.shadowRoot
                .querySelector('#noEsimNetworksMessageWithLink')
                .shadowRoot.querySelector('a');
        assertTrue(!!noEsimNetworksMessageWithLinkAnchor);

        const showEsimCellularSetupPromise =
            eventToPromise('show-cellular-setup', cellularNetworkList);
        noEsimNetworksMessageWithLinkAnchor.click();
        const eSimCellularEvent = await showEsimCellularSetupPromise;
        assertEquals(
            eSimCellularEvent.detail.pageName,
            CellularSetupPageName.ESIM_FLOW_UI);
      });

  [true, false].forEach(shouldEnableSmdsSupport => {
    test('Install pending eSIM profile depending on feature flag', async () => {
      eSimManagerRemote.addEuiccForTest(1);
      init();
      addESimSlot();
      setIsSmdsSupportEnabled(shouldEnableSmdsSupport);

      cellularNetworkList.isConnectedToNonCellularNetwork = true;
      await flushAsync();

      let eSimNetworkList =
          cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');

      if (shouldEnableSmdsSupport) {
        assertNull(eSimNetworkList);
        return;
      }
      assertTrue(!!eSimNetworkList);

      flush();
      const listItem =
          eSimNetworkList.shadowRoot.querySelector('network-list-item');
      assertTrue(!!listItem);
      const installButton = listItem.shadowRoot.querySelector('#installButton');
      assertTrue(!!installButton);
      installButton.click();

      await flushAsync();
      // eSIM network list should now be hidden and link showing.
      eSimNetworkList =
          cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');
      assertNull(eSimNetworkList);
      const noEsimNetworksMessageWithLinkAnchor =
          cellularNetworkList.shadowRoot
              .querySelector('#noEsimNetworksMessageWithLink')
              .shadowRoot.querySelector('a');
      assertTrue(!!noEsimNetworksMessageWithLinkAnchor);
    });
  });

  test(
      'Hide esim section when no EUICC is found or no eSIM slots', async () => {
        init();
        setManagedPropertiesForTest(NetworkType.kCellular, [
          OncMojo.getDefaultManagedProperties(NetworkType.kTether, 'tether1'),
        ]);
        flush();
        await flushAsync();
        // The list should be hidden with no EUICC or eSIM slots.
        assertFalse(
            !!cellularNetworkList.shadowRoot.querySelector('#esimNetworkList'));

        // Add an eSIM slot.
        await addESimSlot();
        // The list should still be hidden.
        assertFalse(
            !!cellularNetworkList.shadowRoot.querySelector('#esimNetworkList'));

        // Add an EUICC.
        eSimManagerRemote.addEuiccForTest(1);
        await flushAsync();
        // The list should now be showing
        assertTrue(
            !!cellularNetworkList.shadowRoot.querySelector('#esimNetworkList'));

        // Remove the eSIM slot
        clearSimSlots();
        // The list should be hidden again.
        assertFalse(
            !!cellularNetworkList.shadowRoot.querySelector('#esimNetworkList'));
      });

  test('Hide pSIM section when no pSIM slots', async () => {
    init();
    setManagedPropertiesForTest(NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(NetworkType.kTether, 'tether1'),
    ]);
    await flushAsync();
    assertFalse(
        !!cellularNetworkList.shadowRoot.querySelector('#pSimNoNetworkFound'));

    addPSimSlot();
    assertTrue(
        !!cellularNetworkList.shadowRoot.querySelector('#pSimNoNetworkFound'));

    clearSimSlots();
    assertFalse(
        !!cellularNetworkList.shadowRoot.querySelector('#pSimNoNetworkFound'));
  });

  test(
      'Show pSIM section when no pSIM slots but pSIM networks available',
      async () => {
        eSimManagerRemote.addEuiccForTest(2);
        init();

        setManagedPropertiesForTest(NetworkType.kCellular, [
          OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'pSimCellular1'),
          OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'pSimcellular2'),
        ]);

        await flushAsync();

        const pSimNetworkList =
            cellularNetworkList.shadowRoot.querySelector('#psimNetworkList');
        assertTrue(!!pSimNetworkList);

        assertEquals(2, pSimNetworkList.networks.length);
      });

  test('Hide instant tethering section when not enabled', async () => {
    init();
    assertFalse(!!cellularNetworkList.shadowRoot.querySelector(
        '#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        MultiDeviceFeatureState.ENABLED_BY_USER);
    await flushAsync();
    assertTrue(!!cellularNetworkList.shadowRoot.querySelector(
        '#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        MultiDeviceFeatureState.UNAVAILABLE_NO_VERIFIED_HOST);
    await flushAsync();
    assertFalse(!!cellularNetworkList.shadowRoot.querySelector(
        '#tetherNetworksNotSetup'));
  });

  test(
      'Allow only managed cellular networks should hide pending eSIM networks',
      async () => {
        eSimManagerRemote.addEuiccForTest(1);
        init();
        addESimSlot();
        cellularNetworkList.globalPolicy = {
          allowOnlyPolicyCellularNetworks: false,
        };
        await flushAsync();
        let eSimNetworkList =
            cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');
        assertTrue(!!eSimNetworkList);

        flush();

        const listItem =
            eSimNetworkList.shadowRoot.querySelector('network-list-item');
        assertTrue(!!listItem);
        const installButton =
            listItem.shadowRoot.querySelector('#installButton');
        assertTrue(!!installButton);

        cellularNetworkList.globalPolicy = {
          allowOnlyPolicyCellularNetworks: true,
        };
        eSimManagerRemote.addEuiccForTest(1);
        addESimSlot();
        await flushAsync();
        eSimNetworkList =
            cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');
        assertFalse(!!eSimNetworkList);
      });

  test(
      'Fire show toast event if download profile clicked without ' +
          'non-cellular connection.',
      async () => {
        eSimManagerRemote.addEuiccForTest(1);
        init();
        addESimSlot();
        cellularNetworkList.isConnectedToNonCellularNetwork = false;
        await flushAsync();

        const eSimNetworkList =
            cellularNetworkList.shadowRoot.querySelector('#esimNetworkList');
        assertTrue(!!eSimNetworkList);

        flush();

        const listItem =
            eSimNetworkList.shadowRoot.querySelector('network-list-item');
        assertTrue(!!listItem);
        const installButton =
            listItem.shadowRoot.querySelector('#installButton');
        assertTrue(!!installButton);

        const showErrorToastPromise =
            eventToPromise('show-error-toast', cellularNetworkList);
        installButton.click();

        const showErrorToastEvent = await showErrorToastPromise;
        assertEquals(
            showErrorToastEvent.detail,
            cellularNetworkList.i18n('eSimNoConnectionErrorToast'));
      });

  test(
      'No network eSIM', async () => {
        eSimManagerRemote.addEuiccForTest(0);
        init();
        cellularNetworkList.deviceState = {
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          inhibitReason: InhibitReason.kNotInhibited,
        };
        cellularNetworkList.globalPolicy = {
          allowOnlyPolicyCellularNetworks: true,
        };
        addESimSlot();
        await flushAsync();

        const getAddEsimLink = () => {
          return cellularNetworkList.shadowRoot.querySelector(
              '#noEsimNetworksMessageWithLink');
        };
        const getNoEsimFoundMessage = () => {
          return cellularNetworkList.shadowRoot.querySelector(
              '#noEsimNetworksMessage');
        };

        cellularNetworkList.globalPolicy = {
          allowOnlyPolicyCellularNetworks: true,
        };
        await flushAsync();

        assertTrue(!!getAddEsimLink());
        assertTrue(!!getNoEsimFoundMessage());
        assertTrue(getAddEsimLink().hidden);
        assertFalse(getNoEsimFoundMessage().hidden);

        assertEquals(
            getNoEsimFoundMessage().textContent.trim(),
            cellularNetworkList.i18n('eSimNetworkNotSetup'));

        cellularNetworkList.globalPolicy = {
          allowOnlyPolicyCellularNetworks: false,
        };
        await flushAsync();

        assertTrue(!!getAddEsimLink());
        assertTrue(!!getNoEsimFoundMessage());
        assertFalse(getAddEsimLink().hidden);
        assertTrue(getNoEsimFoundMessage().hidden);

        for (const inhibitReason
                 of [InhibitReason.kNotInhibited,
                     InhibitReason.kInstallingProfile,
                     InhibitReason.kRenamingProfile,
                     InhibitReason.kRemovingProfile,
                     InhibitReason.kConnectingToProfile,
                     InhibitReason.kRefreshingProfileList,
                     InhibitReason.kResettingEuiccMemory,
                     InhibitReason.kDisablingProfile,
                     InhibitReason.kRequestingAvailableProfiles]) {
          cellularNetworkList.set(
              'cellularDeviceState.inhibitReason', inhibitReason);
          await flushAsync();

          const noEsimNetworksMessageWithLink = getAddEsimLink();
          const noEsimFoundMessage = getNoEsimFoundMessage();

          assertTrue(!!noEsimNetworksMessageWithLink);
          assertTrue(!!noEsimFoundMessage);

          if (inhibitReason === InhibitReason.kNotInhibited) {
            assertFalse(noEsimNetworksMessageWithLink.hidden);
            assertTrue(noEsimFoundMessage.hidden);
          } else if (
              inhibitReason === InhibitReason.kInstallingProfile ||
              inhibitReason === InhibitReason.kRefreshingProfileList ||
              inhibitReason === InhibitReason.kRequestingAvailableProfiles) {
            assertTrue(noEsimNetworksMessageWithLink.hidden);
            assertTrue(noEsimFoundMessage.hidden);
          } else {
            assertTrue(noEsimNetworksMessageWithLink.hidden);
            assertFalse(noEsimFoundMessage.hidden);
          }
        }
      });

  test('Fire show cellular setup event on add cellular clicked', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_esim1');
    eSimNetwork1.typeProperties.cellular.eid =
        '11111111111111111111111111111111';
    setManagedPropertiesForTest(NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular1'),
      eSimNetwork1,
    ]);
    cellularNetworkList.cellularDeviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    };
    addESimSlot();
    cellularNetworkList.globalPolicy = {
      allowOnlyPolicyCellularNetworks: true,
    };
    await flushAsync();

    // When policy is enabled add cellular button should be disabled, and policy
    // indicator should be shown.
    let addESimButton =
        cellularNetworkList.shadowRoot.querySelector('#addESimButton');
    assertTrue(!!addESimButton);
    assertTrue(addESimButton.disabled);
    let policyIcon =
        cellularNetworkList.shadowRoot.querySelector('cr-policy-indicator');
    assertTrue(!!policyIcon);
    assertFalse(policyIcon.hidden);

    cellularNetworkList.globalPolicy = {
      allowOnlyPolicyCellularNetworks: false,
    };

    await flushAsync();
    addESimButton =
        cellularNetworkList.shadowRoot.querySelector('#addESimButton');
    assertTrue(!!addESimButton);
    assertFalse(addESimButton.disabled);
    policyIcon =
        cellularNetworkList.shadowRoot.querySelector('cr-policy-indicator');
    assertTrue(!!policyIcon);
    assertTrue(policyIcon.hidden);

    // When device is inhibited add cellular button should be disabled.
    cellularNetworkList.cellularDeviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kInstallingProfile,
    };
    addESimSlot();
    await flushAsync();
    assertTrue(!!addESimButton);
    assertTrue(addESimButton.disabled);

    // Device is not inhibited and policy is also false add cellular button
    // should be enabled
    cellularNetworkList.cellularDeviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    };
    addESimSlot();
    await flushAsync();
    assertTrue(!!addESimButton);
    assertFalse(addESimButton.disabled);

    const showCellularSetupPromise =
        eventToPromise('show-cellular-setup', cellularNetworkList);
    addESimButton.click();
    await Promise.all([showCellularSetupPromise, flushTasks()]);
  });

  test('Disable no esim link when cellular device is inhibited', async () => {
    eSimManagerRemote.addEuiccForTest(0);
    init();
    cellularNetworkList.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    };
    addESimSlot();

    await flushAsync();

    const noEsimNetworksMessageWithLink =
        cellularNetworkList.shadowRoot.querySelector(
            '#noEsimNetworksMessageWithLink');
    assertFalse(noEsimNetworksMessageWithLink.linkDisabled);

    cellularNetworkList.cellularDeviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kResettingEuiccMemory,
    };
    addESimSlot();
    await flushAsync();
    assertTrue(noEsimNetworksMessageWithLink.linkDisabled);
  });

  test('Show inhibited subtext and spinner when inhibited', async () => {
    eSimManagerRemote.addEuiccForTest(0);
    init();
    cellularNetworkList.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    };
    addESimSlot();
    cellularNetworkList.canShowSpinner = true;
    await flushAsync();

    const inhibitedSubtext =
        cellularNetworkList.shadowRoot.querySelector('#inhibitedSubtext');
    const getInhibitedSpinner = () => {
      return cellularNetworkList.shadowRoot.querySelector('#inhibitedSpinner');
    };
    assertTrue(inhibitedSubtext.hidden);
    assertTrue(!!getInhibitedSpinner());
    assertFalse(getInhibitedSpinner().active);

    cellularNetworkList.cellularDeviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kInstallingProfile,
    };
    addESimSlot();
    await flushAsync();
    assertFalse(inhibitedSubtext.hidden);
    assertTrue(getInhibitedSpinner().active);

    // Do not show inihibited spinner if cellular setup dialog is open.
    cellularNetworkList.canShowSpinner = false;
    await flushAsync();
    assertFalse(!!getInhibitedSpinner());
  });

  test(
      'Inhibited subtext gets updated when inhibit reason changes',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        init();

        // Put the device under the inhibited status with kRefreshingProfileList
        // reason first.
        cellularNetworkList.cellularDeviceState = {
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          inhibitReason: InhibitReason.kRefreshingProfileList,
        };
        addESimSlot();
        cellularNetworkList.canShowSpinner = true;
        await flushAsync();

        const inhibitedSubtext =
            cellularNetworkList.shadowRoot.querySelector('#inhibitedSubtext');
        const getInhibitedSpinner = () => {
          return cellularNetworkList.shadowRoot.querySelector(
              '#inhibitedSpinner');
        };
        assertFalse(inhibitedSubtext.hidden);
        assertTrue(
            inhibitedSubtext.textContent.includes(cellularNetworkList.i18n(
                'cellularNetworRefreshingProfileListProfile')));
        assertTrue(getInhibitedSpinner().active);

        // When device inhibit reason changes from kRefreshingProfileList to
        // kInstallingProfile, the inhibited subtext should also get updated to
        // reflect that.
        cellularNetworkList.cellularDeviceState = {
          inhibitReason: InhibitReason.kInstallingProfile,
        };
        addESimSlot();
        await flushAsync();
        assertFalse(inhibitedSubtext.hidden);
        assertTrue(inhibitedSubtext.textContent.includes(
            cellularNetworkList.i18n('cellularNetworkInstallingProfile')));
        assertTrue(getInhibitedSpinner().active);
      });
});
