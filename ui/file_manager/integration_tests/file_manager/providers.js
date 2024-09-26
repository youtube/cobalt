// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getHistogramCount, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {IGNORE_APP_ERRORS, remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Returns provider name of the given testing provider manifest viz., the
 * the value of the name field in the |manifest| file.
 * @param {string} manifest Testing provider manifest file name.
 * @return {string} Testing provider name.
 */
function getProviderNameForTest(manifest) {
  if (manifest === 'manifest.json') {
    return 'Files Testing Provider test extension';
  }
  if (manifest === 'manifest_multiple_mounts.json') {
    return 'Files Testing Provider multiple mounts test extension';
  }
  if (manifest === 'manifest_source_device.json') {
    return 'Files Testing Provider device test extension';
  }
  if (manifest === 'manifest_source_file.json') {
    return 'Files Testing Provider file test extension';
  }

  throw new Error('unknown mainfest: '.concat(manifest));
}

/**
 * Initializes the provider extension.
 * @param {string} manifest The manifest name of testing provider extension
 *     to launch for the test case.
 */
async function setUpProvider(manifest) {
  await sendTestMessage({name: 'launchProviderExtension', manifest: manifest});
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  return appId;
}

/**
 * Clicks on the "Services" menu button.
 */
async function showProvidersMenu(appId) {
  const providersMenuItem = '#gear-menu-providers:not([hidden])';

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button']),
      'fakeMouseClick failed');

  // Wait for providers menu item to appear.
  await remoteCall.waitForElement(appId, providersMenuItem);

  // Click the menu item.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [providersMenuItem]),
      'fakeMouseClick failed');
}

/**
 * Confirms that a provided volume is mounted.
 */
async function confirmVolume(appId, ejectExpected) {
  await remoteCall.waitForElement(
      appId, '.tree-row .icon[volume-type-icon="provided"]');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['.tree-row .icon[volume-type-icon="provided"]']),
      'fakeMouseClick failed');


  await remoteCall.waitForElement(
      appId, '.tree-row[selected] .icon[volume-type-icon="provided"]');
  if (ejectExpected) {
    await remoteCall.waitForElement(appId, '.tree-row[selected] .root-eject');
  } else {
    await remoteCall.waitForElementLost(
        appId, '.tree-row[selected] .root-eject');
  }
}

/**
 * Tests that a provided extension with |manifest| is mountable via the menu
 * button.
 *
 * @param {boolean} multipleMounts Whether multiple mounts are supported by
 *     the providing extension.
 * @param {string} manifest Name of the manifest file for the providing
 *     extension.
 */
async function requestMountInternal(multipleMounts, manifest) {
  const providerName = getProviderNameForTest(manifest);
  const appId = await setUpProvider(manifest);
  await showProvidersMenu(appId);

  // Wait for providers menu to appear.
  let result = await remoteCall.waitForElement(
      appId, '#providers-menu:not([hidden]) cr-menu-item:first-child span');

  // Click to install test provider.
  chrome.test.assertEq(providerName, result.text);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['#providers-menu cr-menu-item:first-child span']),
      'fakeMouseClick failed');

  await confirmVolume(appId, false /* ejectExpected */);

  // If multipleMounts we display the providers menu and check the provider is
  // still listed.
  if (multipleMounts) {
    await showProvidersMenu(appId);
    const selector = '#providers-menu:not([hidden]) cr-menu-item:first-child ' +
        'span';
    result = await remoteCall.waitForElement(appId, selector);
    chrome.test.assertEq(providerName, result.text);
    return;
  }

  const isSmbEnabled = await sendTestMessage({name: 'isSmbEnabled'}) === 'true';
  if (!isSmbEnabled) {
    return;
  }

  // If !multipleMounts but isSmbEnabled, we display the provider menu and
  // check the provider is not listed.
  await showProvidersMenu(appId);
  const selector = '#providers-menu:not([hidden]) cr-menu-item:first-child ' +
      'span';
  result = await remoteCall.waitForElement(appId, selector);
  chrome.test.assertFalse(providerName === result.text);
}

/**
 * Tests that a provided extension with |manifest| is not available in the
 * providers menu, but it's mounted automatically.
 *
 * @param {string} manifest Name of the manifest file for the providing
 *     extension.
 */
async function requestMountNotInMenuInternal(manifest) {
  const appId = await setUpProvider(manifest);
  await confirmVolume(appId, true /* ejectExpected */);

  const isSmbEnabled = await sendTestMessage({name: 'isSmbEnabled'}) === 'true';

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button']),
      'fakeMouseClick failed');

  // The providers menu item should be hidden if Smb is disabled, since there
  // are no providers to show.
  const providersMenuItem = isSmbEnabled ?
      '#gear-menu-providers:not([hidden])' :
      '#gear-menu-providers[hidden]';
  const element = await remoteCall.waitForElement(appId, providersMenuItem);

  if (!isSmbEnabled) {
    return;
  }

  const isDevtoolsCoverageActive =
      await sendTestMessage({name: 'isDevtoolsCoverageActive'});
  const expectedServicesText = isDevtoolsCoverageActive === 'true' ?
      '$i18n{SHOW_PROVIDERS_BUTTON_LABEL}' :
      'Services';

  // Since a provider is installed (here isSmbEnabled), we need to test that
  // 'providers-menu' sub-menu does not contain the |manifest| provider.
  chrome.test.assertTrue(isSmbEnabled);
  chrome.test.assertEq(expectedServicesText, element.text);
  chrome.test.assertEq(
      '#show-providers-submenu', element.attributes['command']);
  chrome.test.assertEq('#providers-menu', element.attributes['sub-menu']);

  // Extract 'providers-menu' sub-menu items.
  const selector = ['#providers-menu[hidden] cr-menu-item'];
  const submenu =
      await remoteCall.callRemoteTestUtil('queryAllElements', appId, selector);

  // Check the sub-menu do not contain the |manifest| provider.
  chrome.test.assertEq(1, submenu.length);
  chrome.test.assertEq('SMB file share', submenu[0].text);
}

/**
 * Tests mounting a single mount point in the button menu.
 */
testcase.requestMount = () => {
  const multipleMounts = false;
  return requestMountInternal(multipleMounts, 'manifest.json');
};

/**
 * Tests mounting multiple mount points in the button menu.
 */
testcase.requestMountMultipleMounts = () => {
  const multipleMounts = true;
  return requestMountInternal(multipleMounts, 'manifest_multiple_mounts.json');
};

/**
 * Tests mounting a device not present in the button menu.
 */
testcase.requestMountSourceDevice = () => {
  return requestMountNotInMenuInternal('manifest_source_device.json');
};

/**
 * Tests mounting a file not present in the button menu.
 */
testcase.requestMountSourceFile = () => {
  return requestMountNotInMenuInternal('manifest_source_file.json');
};

/**
 * Tests that pressing the eject button on a FSP adds a message to screen
 * reader.
 */
testcase.providerEject = async () => {
  const manifest = 'manifest_source_file.json';
  const appId = await setUpProvider(manifest);

  // Click to eject Test (1) provider/volume.
  const ejectQuery =
      ['#directory-tree [volume-type-for-testing="provided"] .root-eject'];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ejectQuery),
      'click eject failed');

  // Wait a11y-msg to have some text.
  await remoteCall.waitForElement(appId, '#a11y-msg:not(:empty)');

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);

  // Check that opening the file was announced to screen reader.
  chrome.test.assertTrue(a11yMessages instanceof Array);
  chrome.test.assertEq(1, a11yMessages.length);
  chrome.test.assertEq('Test (1) has been ejected.', a11yMessages[0]);

  // Note: We disable app error checking because sometimes there are
  // JS errors due to volume related actions performed while volume is
  // ejected.
  return IGNORE_APP_ERRORS;
};

/**
 * Tests mounting a file system provider emits only a single UMA when running
 * from either the SWA or Chrome app.
 */
testcase.deduplicatedUmaMetricForFileSystemProviders = async () => {
  const umaMetricName = 'FileBrowser.FileSystemProviderMounted';
  const testProviderMetricEnumValue = 0;  // UNKNOWN = 0.

  let mountedVolumeCount = 0;
  chrome.test.assertEq(
      0, mountedVolumeCount, 'Unexpected value in UMA metric for mounted FSPs');

  // Setup Files app before loading the File System Provider. When testing the
  // SWA records only 1 mount event, if the FSP is loaded prior to the window
  // starting, it will always miss the load event leading to a false positive
  // test result.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Setup the FSP and wait for the volume to appear in the directory tree.
  await sendTestMessage({
    name: 'launchProviderExtension',
    manifest: 'manifest_source_device.json',
  });
  await confirmVolume(appId, true /* ejectExpected */);

  // Assert the histogram for an unknown FSP is incremented by 1.
  mountedVolumeCount =
      await getHistogramCount(umaMetricName, testProviderMetricEnumValue);
  chrome.test.assertEq(
      1, mountedVolumeCount, 'Unexpected value in UMA metric for mounted FSPs');
};
