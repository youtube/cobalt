// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {ConfirmationDialogType, CrSettingsPrefs, GoogleDriveBrowserProxy, GoogleDrivePageCallbackRouter, GoogleDrivePageHandlerRemote, GoogleDrivePageRemote, SettingsGoogleDriveSubpageElement, SettingsPrefsElement, SettingsToggleButtonElement, Stage} from 'chrome://os-settings/chromeos/os_settings.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertAsync, querySelectorShadow} from '../utils.js';

/**
 * A fake BrowserProxy implementation that enables switching out the real one to
 * mock various mojo responses.
 */
class GoogleDriveTestBrowserProxy extends TestBrowserProxy implements
    GoogleDriveBrowserProxy {
  handler: TestMock<GoogleDrivePageHandlerRemote>&GoogleDrivePageHandlerRemote;

  observer: GoogleDrivePageCallbackRouter;

  observerRemote: GoogleDrivePageRemote;

  constructor() {
    super(['calculateRequiredSpace', 'getTotalPinnedSize', 'clearPinnedFiles']);
    this.handler = TestMock.fromClass(GoogleDrivePageHandlerRemote);
    this.observer = new GoogleDrivePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();
  }
}

/**
 * Generate the expected text for space available.
 */
function generateRequiredSpaceText(
    requiredSpace: string, remainingSpace: string): string {
  return `This will use about ${requiredSpace} leaving ${
      remainingSpace} available.`;
}

suite('<settings-google-drive-subpage>', function() {
  let page: SettingsGoogleDriveSubpageElement;
  let prefElement: SettingsPrefsElement;
  let connectDisconnectButton: CrButtonElement;
  let testBrowserProxy: GoogleDriveTestBrowserProxy;
  let bulkPinningToggle: SettingsToggleButtonElement;
  let offlineStorageSubtitle: HTMLDivElement;
  let clearOfflineStorageButton: CrButtonElement;

  /**
   * Helper to ensure a confirmation dialog is showing, retrieve a button in the
   * dialog and click it, then assert the dialog has disappeared.
   */
  const clickConfirmationDialogButton =
      async(selector: string): Promise<void> => {
    const getButton = () => querySelectorShadow(
                                page.shadowRoot!,
                                [
                                  'settings-drive-confirmation-dialog',
                                  selector,
                                ]) as CrButtonElement |
        null;

    // Ensure some dialog is showing.
    await assertAsync(
        () => page.dialogType !== ConfirmationDialogType.NONE, 5000);

    // Ensure the button requested is showing.
    await assertAsync(() => getButton() !== null);

    // Click the button and wait for the dialog to disappear.
    getButton()!.click();
    await assertAsync(
        () => page.dialogType === ConfirmationDialogType.NONE, 5000);
  };

  setup(async function() {
    testBrowserProxy = new GoogleDriveTestBrowserProxy();
    GoogleDriveBrowserProxy.setInstance(testBrowserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-google-drive-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();

    connectDisconnectButton =
        querySelectorShadow(
            page.shadowRoot!, ['#driveConnectDisconnect', 'cr-button']) as
        CrButtonElement;

    bulkPinningToggle =
        querySelectorShadow(page.shadowRoot!, ['#driveBulkPinning']) as
        SettingsToggleButtonElement;

    offlineStorageSubtitle = page.shadowRoot!.querySelector<HTMLDivElement>(
        '#drive-offline-storage-row .secondary')!;

    clearOfflineStorageButton = page.shadowRoot!.querySelector<CrButtonElement>(
        '#drive-offline-storage-row cr-button')!;
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
  });

  test('drive connect label is updated when pref is changed', function() {
    // Update the preference and ensure the text has the value "Connect
    // account".
    page.setPrefValue('gdata.disabled', true);
    flush();
    assertEquals(
        connectDisconnectButton!.textContent!.trim(), 'Connect account');

    // Update the preference and ensure the text has the value "Disconnect".
    page.setPrefValue('gdata.disabled', false);
    flush();
    assertEquals(connectDisconnectButton!.textContent!.trim(), 'Disconnect');
  });

  test('confirming drive disconnect updates pref', async function() {
    page.setPrefValue('gdata.disabled', false);
    flush();

    // Click the connect disconnect button.
    connectDisconnectButton.click();

    // Click the disconnect button.
    await clickConfirmationDialogButton('.action-button');

    // Ensure after clicking the disconnect button the preference is true
    // (timeout after 5s).
    await assertAsync(() => page.getPref('gdata.disabled').value, 5000);
  });

  test(
      'cancelling drive disconnect confirmation dialog doesnt update pref',
      async function() {
        page.setPrefValue('gdata.disabled', false);
        flush();

        // Click the connect disconnect button.
        connectDisconnectButton.click();

        // Wait for the disconnect confirmation button to be visible.
        await clickConfirmationDialogButton('.cancel-button');

        // Ensure after cancelling the dialog the preference is unchanged.
        await assertAsync(() => !page.getPref('gdata.disabled').value, 5000);
      });

  test(
      'clicking the toggle updates the bulk pinning preference',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);
        flush();

        // Toggle the bulk pinning toggle.
        bulkPinningToggle.click();

        // Ensure the bulk pinning preference is enabled (timeout after 5s).
        await assertAsync(
            () => page.getPref('drivefs.bulk_pinning_enabled').value, 5000);
      });

  test(
      'progress sent via the browser proxy updates the sub title text',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);

        /**
         * Helper method to retrieve the subtitle text from the bulk pinning
         * label.
         */
        const expectSubTitleText = async (fn: (text: string) => boolean) => {
          let subTitleElement: HTMLElement|null;
          await assertAsync(() => {
            subTitleElement =
                bulkPinningToggle.shadowRoot!.querySelector<HTMLElement>(
                    '#sub-label-text');
            return subTitleElement !== null && fn(subTitleElement!.innerText);
          }, 5000);
        };


        // Expect the subtitle text does not include required space when no
        // values have been returned from the page handler.
        const requiredSpaceText =
            generateRequiredSpaceText('512 MB', '1,024 KB');
        await expectSubTitleText(
            subTitle => !subTitle.includes(requiredSpaceText));

        // Mock space values and the `kSuccess` stage via the browser proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: '1,024 KB',
          requiredSpace: '512 MB',
          stage: Stage.kSuccess,
          isError: false,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Ensure the sub title text gets updated with the space values.
        await expectSubTitleText(
            subTitle => subTitle.includes(requiredSpaceText));

        // Mock a failure case via the browser proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: '1,024 KB',
          requiredSpace: '512 MB',
          stage: Stage.kCannotGetFreeSpace,
          isError: true,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Ensure the sub title textremoves the space values.
        await expectSubTitleText(
            subTitle => !subTitle.includes(requiredSpaceText));
      });

  test('disabling bulk pinning shows confirmation dialog', async function() {
    page.setPrefValue('drivefs.bulk_pinning_enabled', true);
    flush();

    // Click the bulk pinning toggle.
    bulkPinningToggle.click();

    // Wait for the confirmation dialog to appear and click the cancel button.
    await clickConfirmationDialogButton('.cancel-button');

    // Expect the preference to not be changed and the toggle to stay checked.
    assertTrue(
        page.getPref('drivefs.bulk_pinning_enabled').value,
        'Pinning pref should be true');
    assertTrue(bulkPinningToggle.checked, 'Pinning toggle should be true');

    // Click the bulk pinning toggle.
    bulkPinningToggle.click();

    // Wait for the confirmation dialog to appear and click the "Turn off"
    // button.
    await assertAsync(
        () => page.dialogType === ConfirmationDialogType.BULK_PINNING_DISABLE,
        5000);
    await clickConfirmationDialogButton('.action-button');

    assertFalse(
        page.getPref('drivefs.bulk_pinning_enabled').value,
        'Pinning pref should be false');
    assertFalse(bulkPinningToggle.checked, 'Pinning toggle should be false');
  });

  test(
      'atempting to enable bulk pinning when no free space shows dialog',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);

        // Mock space values and the `kNotEnoughSpace` stage via the browser
        // proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: '512 MB',
          requiredSpace: '1,024 MB',
          stage: Stage.kNotEnoughSpace,
          isError: true,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Wait for the page to update the progress information.
        await assertAsync(() => page.remainingSpace === '512 MB');

        // Click the bulk pinning toggle.
        bulkPinningToggle.click();

        // Wait for the confirmation dialog to appear and assert the toggle
        // hasn't been enabled when the dialog is visible, then click the
        // "Cancel" button.
        await assertAsync(
            () => page.dialogType ===
                ConfirmationDialogType.BULK_PINNING_NOT_ENOUGH_SPACE,
            5000);
        await assertAsync(() => !bulkPinningToggle.checked);
        await clickConfirmationDialogButton('.cancel-button');

        // Wait for the dialog to be dismissed, then assert the toggle hasn't
        // been checked and the preference hasn't been set.
        await assertAsync(
            () => page.dialogType === ConfirmationDialogType.NONE, 5000);
        assertFalse(
            page.getPref('drivefs.bulk_pinning_enabled').value,
            'Pinning pref should be false');
        assertFalse(
            bulkPinningToggle.checked, 'Pinning toggle should not be toggled');
      });

  test(
      'attempting to enable bulk pinning when no free space shows dialog',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);

        // Mock space values and the `kNotEnoughSpace` stage via the browser
        // proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: 'x',
          requiredSpace: 'y',
          stage: Stage.kCannotGetFreeSpace,
          isError: true,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Wait for the page to update the progress information.
        await assertAsync(() => page.remainingSpace === 'x');

        // Click the bulk pinning toggle.
        bulkPinningToggle.click();

        // Wait for the confirmation dialog to appear and assert the toggle
        // hasn't been enabled when the dialog is visible, then click the
        // "Cancel" button.
        await assertAsync(
            () => page.dialogType ===
                ConfirmationDialogType.BULK_PINNING_UNEXPECTED_ERROR,
            5000);
        await assertAsync(() => !bulkPinningToggle.checked);
        await clickConfirmationDialogButton('.cancel-button');

        // Wait for the dialog to be dismissed, then assert the toggle hasn't
        // been checked and the preference hasn't been set.
        await assertAsync(
            () => page.dialogType === ConfirmationDialogType.NONE, 5000);
        assertFalse(
            page.getPref('drivefs.bulk_pinning_enabled').value,
            'Pinning pref should be false');
        assertFalse(
            bulkPinningToggle.checked, 'Pinning toggle should not be toggled');
      });

  test('free space shows the offline value returned', async function() {
    // Send back a normal pinned size result.
    testBrowserProxy.handler.setResultFor(
        'getTotalPinnedSize', {size: '100 MB'});
    page.onNavigated();
    await assertAsync(() => offlineStorageSubtitle.innerText === '100 MB');

    // Mock an empty pinned size (size is there but an empty string).
    testBrowserProxy.handler.setResultFor('getTotalPinnedSize', {size: ''});
    page.onNavigated();

    await assertAsync(() => offlineStorageSubtitle.innerText === 'Unknown');
  });

  test(
      'clear offline files disabled when bulk pinning enabled',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: 'x',
          requiredSpace: 'y',
          stage: Stage.kStopped,
          isError: false,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        await assertAsync(() => !clearOfflineStorageButton.disabled);

        page.setPrefValue('drivefs.bulk_pinning_enabled', true);
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: 'x',
          requiredSpace: 'y',
          stage: Stage.kSyncing,
          isError: false,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        await assertAsync(() => clearOfflineStorageButton.disabled);
      });

  test('when clear offline files clicked show dialog', async function() {
    page.setPrefValue('drivefs.bulk_pinning_enabled', false);

    clearOfflineStorageButton.click();
    await assertAsync(
        () =>
            page.dialogType === ConfirmationDialogType.BULK_PINNING_CLEAR_FILES,
        5000);
    await clickConfirmationDialogButton('.cancel-button');
    assertEquals(testBrowserProxy.handler.getCallCount('clearPinnedFiles'), 0);

    testBrowserProxy.handler.setResultFor(
        'getTotalPinnedSize', {size: '100 MB'});

    clearOfflineStorageButton.click();
    await assertAsync(
        () =>
            page.dialogType === ConfirmationDialogType.BULK_PINNING_CLEAR_FILES,
        5000);
    await clickConfirmationDialogButton('.action-button');
    await assertAsync(
        () => testBrowserProxy.handler.getCallCount('clearPinnedFiles') === 1);
  });
});
