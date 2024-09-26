// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {DialogType} from '../dialog_type.js';
import {ExecuteScriptError} from '../remote_call.js';
import {addEntries, ENTRIES, EntryType, getCaller, getHistogramCount, pending, repeatUntil, RootPath, sanitizeDate, sendTestMessage, TestEntryInfo, wait} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, mountGuestOs, navigateWithDirectoryTree, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, BASIC_ZIP_ENTRY_SET, MODIFIED_ENTRY_SET} from './test_data.js';

/**
 * The tag used to create a safe environment to display the preview.
 */
const previewTag = 'iframe';

/**
 * The JS code used to query the content window for preview.
 */
const contentWindowQuery = 'document.querySelector("iframe").contentWindow';

/**
 * The name of the UMA emitted to track how Quick View is opened.
 * @const {string}
 */
const QuickViewUmaWayToOpenHistogramName = 'FileBrowser.QuickView.WayToOpen';

/**
 * The UMA's enumeration values (must be consistent with enums.xml,
 * previously histograms.xml).
 * @enum {number}
 */
const QuickViewUmaWayToOpenHistogramValues = {
  CONTEXT_MENU: 0,
  SPACE_KEY: 1,
  SELECTION_MENU: 2,
};

/**
 * Checks if dark mode has been turned on or not.
 *
 * @return {!Promise<boolean>} enabled or not.
 */
async function isDarkModeEnabled() {
  const isDarkModeEnabled = await sendTestMessage({name: 'isDarkModeEnabled'});
  return isDarkModeEnabled === 'true';
}

/**
 * Returns the $i18n{} label for the Quick View item |text| if devtools code
 * coverage is enabled. Otherwise, returns |text|.
 *
 * @param {string} text Quick View item text.
 * @return {!Promise<string>}
 */
async function i18nQuickViewLabelText(text) {
  const isDevtoolsCoverageActive =
      await sendTestMessage({name: 'isDevtoolsCoverageActive'});

  if (isDevtoolsCoverageActive !== 'true') {
    return text;
  }

  /** @const {!Object<string, string>} */
  const i18nQuickViewItemTextLabels = {
    // Quick View toolbar button items.
    'Back': 'QUICK_VIEW_CLOSE_BUTTON_LABEL',
    'Delete': 'QUICK_VIEW_DELETE_BUTTON_LABEL',
    'File info': 'QUICK_VIEW_TOGGLE_METADATA_BOX_BUTTON_LABEL',
    'Open': 'QUICK_VIEW_OPEN_IN_NEW_BUTTON_LABEL',

    // Quick View content panel items.
    'No preview available': 'QUICK_VIEW_NO_PREVIEW_AVAILABLE',

    // Quick View metadata box items.
    'Album': 'METADATA_BOX_ALBUM_TITLE',
    'Artist': 'METADATA_BOX_MEDIA_ARTIST',
    'Audio info': 'METADATA_BOX_AUDIO_INFO',
    'Codec': 'METADATA_BOX_CODEC',
    'Created by': 'METADATA_BOX_CREATED_BY',
    'Created time': 'METADATA_BOX_CREATION_TIME',
    'Date modified': 'METADATA_BOX_MODIFICATION_TIME',
    'Device model': 'METADATA_BOX_EXIF_DEVICE_MODEL',
    'Device settings': 'METADATA_BOX_EXIF_DEVICE_SETTINGS',
    'Dimensions': 'METADATA_BOX_DIMENSION',
    'Duration': 'METADATA_BOX_DURATION',
    'File location': 'METADATA_BOX_FILE_LOCATION',
    'Frame rate': 'METADATA_BOX_FRAME_RATE',
    'General info': 'METADATA_BOX_GENERAL_INFO',
    'Genre': 'METADATA_BOX_GENRE',
    'Geography': 'METADATA_BOX_EXIF_GEOGRAPHY',
    'Original location': 'METADATA_BOX_ORIGINAL_LOCATION',
    'Image info': 'METADATA_BOX_IMAGE_INFO',
    'Modified by': 'METADATA_BOX_MODIFIED_BY',
    'Page count': 'METADATA_BOX_PAGE_COUNT',
    'Path': 'METADATA_BOX_FILE_PATH',
    'Size': 'METADATA_BOX_FILE_SIZE',
    'Source': 'METADATA_BOX_SOURCE',
    'Title': 'METADATA_BOX_MEDIA_TITLE',
    'Track': 'METADATA_BOX_TRACK',
    'Type': 'METADATA_BOX_MEDIA_MIME_TYPE',
    'Video info': 'METADATA_BOX_VIDEO_INFO',
    'Year recorded': 'METADATA_BOX_YEAR_RECORDED',
  };

  // Verify |text| has an $i18n{} label in |i18nQuickViewItemTextLabels|.
  const label = i18nQuickViewItemTextLabels[text];
  chrome.test.assertEq('string', typeof label, `Missing: ${text}`);

  // Return the $i18n{} label of |text|.
  return `$i18n{${label}}`;
}

/**
 * Waits for Quick View dialog to be open.
 *
 * @param {string} appId Files app windowId.
 */
async function waitQuickViewOpen(appId) {
  const caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return;
  }

  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog[open]'];
    return checkQuickViewElementsDisplayBlock(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Waits for Quick View dialog to be closed.
 *
 * @param {string} appId Files app windowId.
 */
async function waitQuickViewClose(appId) {
  const caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length == 0 || elements[0].styles.display !== 'none') {
      return pending(caller, 'Waiting for Quick View to close.');
    }
  }

  // Check: the Quick View dialog should not be shown.
  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog:not([open])'];
    return checkQuickViewElementsDisplayNone(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Opens the Quick View dialog on a given file |name|. The file must be
 * present in the Files app file list.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name File name.
 */
async function openQuickView(appId, name) {
  // Select file |name| in the file list.
  await remoteCall.waitUntilSelected(appId, name);

  // Press the space key.
  const space = ['#file-list', ' ', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space),
      'fakeKeyDown failed');

  // Check: the Quick View dialog should be shown.
  return waitQuickViewOpen(appId);
}

/**
 * Opens the Quick View dialog by right clicking on the file |name| and
 * using the "Get Info" command from the context menu.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name File name.
 */
async function openQuickViewViaContextMenu(appId, name) {
  // Right-click the file in the file-list.
  const query = `#file-list [file-name="${name}"]`;
  await remoteCall.waitAndRightClick(appId, query);

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the file-list context menu "Get info" command.
  const getInfoMenuItem = '#file-context-menu:not([hidden]) ' +
      ' [command="#get-info"]:not([hidden])';
  await remoteCall.waitAndClickElement(appId, getInfoMenuItem);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);
}

/**
 * Opens the Quick View dialog with given file |names|. The files must be
 * present and check-selected in the Files app file list.
 *
 * @param {string} appId Files app windowId.
 * @param {Array<string>} names File names.
 */
async function openQuickViewMultipleSelection(appId, names) {
  // Get the file-list rows that are check-selected (multi-selected).
  const selectedRows = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);

  // Check: the selection should contain the given file names.
  chrome.test.assertEq(names.length, selectedRows.length);
  for (let i = 0; i < names.length; i++) {
    chrome.test.assertTrue(
        selectedRows[i].attributes['file-name'].includes(names[i]));
  }

  // Open Quick View via its keyboard shortcut.
  const space = ['#file-list', ' ', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);
}

/**
 * Mount and select USB.
 *
 * @param {string} appId Files app windowId.
 */
async function mountAndSelectUsb(appId) {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
}

/**
 * Assuming that Quick View is currently open per openQuickView above, closes
 * the Quick View dialog.
 *
 * @param {string} appId Files app windowId.
 */
async function closeQuickView(appId) {
  // Click on Quick View to close it.
  const panelElements = ['#quick-view', '#contentPanel'];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [panelElements]),
      'fakeMouseClick failed');

  // Check: the Quick View dialog should not be shown.
  await waitQuickViewClose(appId);
}

/**
 * Assuming that Quick View is currently open per openQuickView above, return
 * the text shown in the QuickView Metadata Box field |name|. If the optional
 * |hidden| is 'hidden', the field |name| should not be visible.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name QuickView Metadata Box field name.
 * @param {string} hidden Whether the field name should be visible.
 *
 * @return {!Promise<string>} text Text value in the field name.
 */
async function getQuickViewMetadataBoxField(appId, name, hidden = '') {
  let filesMetadataBox = 'files-metadata-box';

  /**
   * <files-metadata-box> field rendering is async. The field name has been
   * rendered when its 'metadata' attribute indicates that.
   */
  switch (name) {
    case 'Size':
      filesMetadataBox += '[metadata~="size"]';
      break;
    case 'Date modified':
    case 'Type':
      filesMetadataBox += '[metadata~="mime"]';
      break;
    case 'File location':
      filesMetadataBox += '[metadata~="location"]';
      break;
    case 'Original location':
      filesMetadataBox += '[metadata~="originalLocation"]';
      break;
    default:
      filesMetadataBox += '[metadata~="meta"]';
      break;
  }

  /**
   * The <files-metadata-box> element resides in the #quick-view shadow DOM
   * as a child of the #dialog element.
   */
  const quickViewQuery = ['#quick-view', `#dialog[open] ${filesMetadataBox}`];

  /**
   * The <files-metadata-entry key="name"> element resides in the shadow DOM
   * of the <files-metadata-box>.
   */
  const nameText = await i18nQuickViewLabelText(name);
  quickViewQuery.push(`files-metadata-entry[key="${nameText}"]`);

  /**
   * It has a #value div child in its shadow DOM containing the field value,
   * but if |hidden| was given, the field should not be visible.
   */
  if (hidden !== 'hidden') {
    quickViewQuery.push('#value > div:not([hidden])');
  } else {
    quickViewQuery.push('#box[hidden]');
  }

  const element = await remoteCall.waitForElement(appId, quickViewQuery);
  if (name === 'Date modified') {
    return sanitizeDate(element.text || '');
  } else {
    return element.text;
  }
}

/**
 * Executes a script in the context of a <preview-tag> element and returns its
 * output. Returns undefined when ExecuteScriptError is caught.
 *
 * @param {string} appId App window Id.
 * @param {!Array<string>} query Query to the <preview-tag> element (this is
 *     ignored for SWA).
 * @param {string} statement Javascript statement to be executed within the
 *     <preview-tag>.
 * @return {!Promise<*>}
 */
async function executeJsInPreviewTagAndCatchErrors(appId, query, statement) {
  try {
    return await remoteCall.executeJsInPreviewTag(appId, query, statement);
  } catch (e) {
    if (e instanceof ExecuteScriptError) {
      return undefined;
    } else {
      throw (e);
    }
  }
}

/**
 * Tests opening Quick View on a local downloads file.
 */
testcase.openQuickView = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check the open button is shown.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
};

/**
 * Tests opening Quick View on a local downloads file in an open file dialog.
 */
testcase.openQuickViewDialog = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], [],
      {type: DialogType.SELECT_OPEN_FILE});

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check the open button is not shown as we're in an open file dialog.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button[hidden]']);
};

/**
 * Tests that Quick View opens via the context menu with a single selection.
 */
testcase.openQuickViewViaContextMenuSingleSelection = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select the file in the file list.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Check: clicking the context menu "Get Info" should open Quick View.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);
};

/**
 * Tests that Quick View opens via the context menu when multiple files
 * are selected (file-list check-select mode).
 */
testcase.openQuickViewViaContextMenuCheckSelections = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check: clicking the context menu "Get Info" should open Quick View.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
testcase.closeQuickView = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Close Quick View.
  await closeQuickView(appId);
};

/**
 * Tests opening Quick View on a Drive file.
 */
testcase.openQuickViewDrive = async () => {
  // Open Files app on Drive containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.hello]);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check: the correct mimeType should be displayed (see crbug.com/1067499
  // for details on mimeType differences between Drive and local filesystem).
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);

  // Check: the correct file location should be displayed in Drive.
  const location = await getQuickViewMetadataBoxField(appId, 'File location');
  chrome.test.assertEq('My Drive/hello.txt', location);
};

/**
 * Tests opening Quick View on a Smbfs file.
 */
testcase.openQuickViewSmbfs = async () => {
  const SMBFS_VOLUME_QUERY = '#directory-tree [volume-type-icon="smb"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Populate Smbfs with some files.
  await addEntries(['smbfs'], BASIC_LOCAL_ENTRY_SET);

  // Mount Smbfs volume.
  await sendTestMessage({name: 'mountSmbfs'});

  // Wait for the Smbfs volume to mount.
  await remoteCall.waitForElement(appId, SMBFS_VOLUME_QUERY);

  // Click to open the Smbfs volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [SMBFS_VOLUME_QUERY]),
      'fakeMouseClick failed');

  const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on a USB file.
 */
testcase.openQuickViewUsb = async () => {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a USB file in Quick View.
  await mountAndSelectUsb(appId);
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on a removable partition.
 */
testcase.openQuickViewRemovablePartitions = async () => {
  const PARTITION_QUERY =
      '#directory-tree .tree-children [volume-type-icon="removable"]';
  const caller = getCaller();

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount USB device containing partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for the USB root to be available.
  await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="Drive Label"]');
  await navigateWithDirectoryTree(appId, '/Drive Label');

  // Wait for 2 removable partitions to appear in the directory tree.
  await repeatUntil(async () => {
    const partitions = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, [PARTITION_QUERY]);

    if (partitions.length == 2) {
      return true;
    }
    return pending(
        caller, 'Found %d partitions, waiting for 2.', partitions.length);
  });

  // Click to open the first partition.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [PARTITION_QUERY]),
      'fakeMouseClick failed');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on an item that was Trashed shows original location
 * instead of the current file location.
 */
testcase.openQuickViewTrash = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  await navigateWithDirectoryTree(appId, '/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Open the file in Quick View.
  await openQuickView(appId, 'hello.txt');

  // Check: the original location should be shown instead of the actual file
  // location.
  const location =
      await getQuickViewMetadataBoxField(appId, 'Original location');
  chrome.test.assertEq('My files/Downloads/hello.txt', location);
};

/**
 * Tests seeing dashes for an empty last_modified for DocumentsProvider.
 */
testcase.openQuickViewLastModifiedMetaData = async () => {
  const documentsProviderVolumeQuery =
      '[has-children="true"] [volume-type-icon="documents_provider"]';

  // Add files to the DocumentsProvider volume.
  await addEntries(['documents_provider'], MODIFIED_ENTRY_SET);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount and then click to open
  // DocumentsProvider Volume.
  await remoteCall.waitAndClickElement(appId, documentsProviderVolumeQuery);

  // Check: the DocumentsProvider files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(MODIFIED_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a DocumentsProvider file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  const lastValidModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq(ENTRIES.hello.lastModifiedTime, lastValidModifiedText);

  await closeQuickView(appId);

  // Open a DocumentsProvider file in Quick View.
  await openQuickView(appId, ENTRIES.invalidLastModifiedDate.nameText);

  // Modified time should be displayed as "--" when it's absent.
  const lastInvalidModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq('--', lastInvalidModifiedText);
};

/**
 * Tests opening Quick View on an MTP file.
 */
testcase.openQuickViewMtp = async () => {
  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount a non-empty MTP volume.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP volume to mount.
  await remoteCall.waitForElement(appId, MTP_VOLUME_QUERY);

  // Click to open the MTP volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the MTP files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open an MTP file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on a Crostini file.
 */
testcase.openQuickViewCrostini = async () => {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a Crostini file in Quick View.
  await mountCrostini(appId);
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on a GuestOS file.
 */
testcase.openQuickViewGuestOs = async () => {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a GuestOS file in Quick View.
  await mountGuestOs(appId, BASIC_LOCAL_ENTRY_SET);
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on an Android file.
 */
testcase.openQuickViewAndroid = async () => {
  // Open Files app on Android files.
  const appId = await openNewWindow(RootPath.ANDROID_FILES);

  // Add files to the Android files volume.
  const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
  await addEntries(['android_files'], entrySet);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: the basic Android file set should appear in the file list.
  let files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Navigate to the Android files '/Documents' directory.
  await navigateWithDirectoryTree(appId, '/My files/Play files/Documents');

  // Check: the 'android.txt' file should appear in the file list.
  files = [ENTRIES.documentsText.getExpectedRow()];
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the Android file in Quick View.
  const documentsFileName = ENTRIES.documentsText.nameText;
  await openQuickView(appId, documentsFileName);
};

/**
 * Tests opening Quick View on an Android file on GuestOS.
 */
testcase.openQuickViewAndroidGuestOs = async () => {
  // Open Files app on Android files.
  const appId = await openNewWindow(RootPath.ANDROID_FILES);

  // Add files to the Android files volume.
  const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
  await addEntries(['android_files'], entrySet);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: the basic Android file set should appear in the file list.
  let files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Navigate to the Android files '/Documents' directory.
  await navigateWithDirectoryTree(appId, '/My files/Play files/Documents');

  // Check: the 'android.txt' file should appear in the file list.
  files = [ENTRIES.documentsText.getExpectedRow()];
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the Android file in Quick View.
  const documentsFileName = ENTRIES.documentsText.nameText;
  await openQuickView(appId, documentsFileName);
};

/**
 * Tests opening Quick View on a DocumentsProvider root.
 */
testcase.openQuickViewDocumentsProvider = async () => {
  const DOCUMENTS_PROVIDER_VOLUME_QUERY =
      '[has-children="true"] [volume-type-icon="documents_provider"]';

  // Add files to the DocumentsProvider volume.
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount.
  await remoteCall.waitForElement(appId, DOCUMENTS_PROVIDER_VOLUME_QUERY);

  // Click to open the DocumentsProvider volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [DOCUMENTS_PROVIDER_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the DocumentsProvider files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a DocumentsProvider file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Wait for the Quick View preview to load and display its content.
  const caller = getCaller();
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Wait until the preview displays the file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    // Check: the content of text file should be shown.
    if (!text || !text[0] || !text[0].includes('chocolate and chips')) {
      return pending(caller, `Waiting for ${previewTag} content.`);
    }
  });

  // Check: the correct size and date modified values should be displayed.
  const sizeText = await getQuickViewMetadataBoxField(appId, 'Size');
  chrome.test.assertEq(ENTRIES.hello.sizeText, sizeText);
  const lastModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq(ENTRIES.hello.lastModifiedTime, lastModifiedText);
};

/**
 * Tests opening Quick View with a local text document identified as text from
 * file sniffing (the first word of the file is "From ", note trailing space).
 */
testcase.openQuickViewSniffedText = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.plainText.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.plainText], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.plainText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);
};

/**
 * Tests opening Quick View with a local text document whose MIME type cannot
 * be identified by MIME type sniffing.
 */
testcase.openQuickViewTextFileWithUnknownMimeType = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the mimeType field should not be displayed.
  await getQuickViewMetadataBoxField(appId, 'Type', 'hidden');
};

/**
 * Tests opening Quick View with a text file containing some UTF-8 encoded
 * characters: crbug.com/1064855
 */
testcase.openQuickViewUtf8Text = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.utf8Text.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.utf8Text], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.utf8Text.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Wait until the preview displays the file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);

    // Check: the content of ENTRIES.utf8Text should be shown.
    if (!text || !text[0] ||
        !text[0].includes('їсти मुझे |∊☀✌✂♁ 🙂\n')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });

  // Check: the correct file size should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Size');
  chrome.test.assertEq('191 bytes', size);
};

/**
 * Tests opening Quick View and scrolling its preview contents which contains a
 * tall text document.
 */
testcase.openQuickViewScrollText = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  function scrollQuickViewTextBy(y) {
    const doScrollBy = `${contentWindowQuery}.scrollBy(0,${y})`;
    return remoteCall.executeJsInPreviewTag(appId, preview, doScrollBy);
  }

  async function checkQuickViewTextScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 150) {
      await scrollQuickViewTextBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
  }

  // Open Files app on Downloads containing ENTRIES.tallText.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Statement to get the Quick View preview scrollY.
  const getScrollY = `${contentWindowQuery}.scrollY`;

  // The initial preview scrollY should be 0.
  await repeatUntil(async () => {
    const scrollY =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getScrollY);
    if (String(scrollY) !== '0') {
      return pending(caller, 'Waiting for preview text to load.');
    }
  });

  // Scroll the preview and verify that it scrolled.
  await repeatUntil(async () => {
    const scrollY =
        await remoteCall.executeJsInPreviewTag(appId, preview, getScrollY);
    return checkQuickViewTextScrollY(scrollY);
  });
};

/**
 * Tests opening Quick View containing a PDF document.
 */
testcase.openQuickViewPdf = async () => {
  const caller = getCaller();

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Open Files app on Downloads containing ENTRIES.tallPdf.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallPdf], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallPdf.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview embed type attribute.
  function checkPdfEmbedType(type) {
    const haveElements = Array.isArray(type) && type.length === 1;
    if (!haveElements || !type[0].toString().includes('pdf')) {
      return pending(caller, 'Waiting for plugin <embed> type.');
    }
    return type[0];
  }
  const type = await repeatUntil(async () => {
    const getType =
        contentWindowQuery + '.document.querySelector("embed").type';
    const type =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getType);
    return checkPdfEmbedType(type);
  });

  // Check: the preview embed type should be PDF mime type.
  chrome.test.assertEq('application/pdf', type);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
};

/**
 * Tests opening Quick View on a PDF document that opens a popup JS dialog.
 */
testcase.openQuickViewPdfPopup = async () => {
  const caller = getCaller();

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Open Files app on Downloads containing ENTRIES.popupPdf.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.popupPdf], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.popupPdf.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview embed type attribute.
  function checkPdfEmbedType(type) {
    const haveElements = Array.isArray(type) && type.length === 1;
    if (!haveElements || !type[0].toString().includes('pdf')) {
      return pending(caller, 'Waiting for plugin <embed> type.');
    }
    return type[0];
  }
  const type = await repeatUntil(async () => {
    const getType =
        contentWindowQuery + '.document.querySelector("embed").type';
    const type =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getType);
    return checkPdfEmbedType(type);
  });

  // Check: the preview embed type should be PDF mime type.
  chrome.test.assertEq('application/pdf', type);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
};

/**
 * Tests that Quick View does not display a PDF file preview when that is
 * disabled by system settings (preferences).
 */
testcase.openQuickViewPdfPreviewsDisabled = async () => {
  const caller = getCaller();

  /**
   * The #innerContentPanel resides in the #quick-view shadow DOM as a child
   * of the #dialog element, and contains the file preview result.
   */
  const contentPanel = ['#quick-view', '#dialog[open] #innerContentPanel'];

  // Disable PDF previews.
  await sendTestMessage({name: 'setPdfPreviewEnabled', enabled: false});

  // Open Files app on Downloads containing ENTRIES.tallPdf.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallPdf], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallPdf.nameText);

  // Get the content panel 'No preview available' item text.
  const noPreviewAvailableText =
      await i18nQuickViewLabelText('No preview available');

  // Wait for the innerContentPanel to load and display its content.
  function checkInnerContentPanel(elements) {
    const haveElements = Array.isArray(elements) && elements.length === 1;
    if (!haveElements || elements[0].styles.display !== 'flex') {
      return pending(caller, 'Waiting for inner content panel to load.');
    }
    // Check: the PDF preview should not be shown.
    chrome.test.assertEq(noPreviewAvailableText, elements[0].text);
    return;
  }
  await repeatUntil(async () => {
    return checkInnerContentPanel(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [contentPanel, ['display']]));
  });

  // Check: the correct file mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
};

/**
 * Tests opening Quick View with a '.mhtml' filename extension.
 */
testcase.openQuickViewMhtml = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', 'files-safe-media[type="html"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.plainText.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.mHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.mHtml.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);

  // Check: the correct file location should be displayed.
  const location = await getQuickViewMetadataBoxField(appId, 'File location');
  chrome.test.assertEq('My files/Downloads/page.mhtml', location);
};

/**
 * Tests opening Quick View and scrolling its preview contents which contains a
 * tall html document.
 */
testcase.openQuickViewScrollHtml = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="html"]', previewTag];

  function scrollQuickViewHtmlBy(y) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall.executeJsInPreviewTag(appId, preview, doScrollBy);
  }

  async function checkQuickViewHtmlScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      await scrollQuickViewHtmlBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
  }

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallHtml.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewHtmlLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewHtmlLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the Quick View preview scrollY.
  const getScrollY = 'window.scrollY';

  // The initial preview scrollY should be 0.
  await repeatUntil(async () => {
    const scrollY =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getScrollY);
    if (String(scrollY) !== '0') {
      return pending(caller, `Waiting for preview text to load.`);
    }
  });

  // Scroll the preview and verify that it scrolled.
  await repeatUntil(async () => {
    const scrollY =
        await remoteCall.executeJsInPreviewTag(appId, preview, getScrollY);
    return checkQuickViewHtmlScrollY(scrollY);
  });

  // Check: the mimeType field should not be displayed.
  await getQuickViewMetadataBoxField(appId, 'Type', 'hidden');
};

/**
 * Tests opening Quick View on an html document to verify that the background
 * color of the <files-safe-media type="html"> that contains the preview is
 * solid white.
 */
testcase.openQuickViewBackgroundColorHtml = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM. This test only needs to
   * examine the <files-safe-media> element.
   */
  const fileSafeMedia = ['#quick-view', 'files-safe-media[type="html"]'];

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallHtml.nameText);

  // Get the <files-safe-media type='html'> backgroundColor style.
  function getFileSafeMediaBackgroundColor(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].styles.backgroundColor) {
      return pending(caller, 'Waiting for <files-safe-media> element.');
    }
    return elements[0].styles.backgroundColor;
  }
  const backgroundColor = await repeatUntil(async () => {
    const styles = ['display', 'backgroundColor'];
    return getFileSafeMediaBackgroundColor(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [fileSafeMedia, styles]));
  });

  // Check: the <files-safe-media> backgroundColor: var(--cros-bg-color).
  if (await isDarkModeEnabled()) {
    chrome.test.assertEq('rgb(32, 33, 36)', backgroundColor);
  } else {
    chrome.test.assertEq('rgb(255, 255, 255)', backgroundColor);
  }
};

/**
 * Tests opening Quick View containing an audio file without album preview.
 */
testcase.openQuickViewAudio = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  /**
   * The album artwork preview resides in the <files-safe-media> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const albumArtworkPreview = ['#quick-view', '#audio-artwork'];

  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.beautiful.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the audio artwork is not shown on the preview page.
  const albumArtworkElements = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, [albumArtworkPreview, ['display']]);
  const hasArtworkElements =
      Array.isArray(albumArtworkElements) && albumArtworkElements.length > 0;
  chrome.test.assertFalse(hasArtworkElements);

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';

  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('audio/ogg', mimeType);
};

/**
 * Tests opening Quick View containing an audio file on Drive.
 */
testcase.openQuickViewAudioOnDrive = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.beautiful.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }
};

/**
 * Tests opening Quick View containing an audio file that has an album art
 * image in its metadata.
 */
testcase.openQuickViewAudioWithImageMetadata = async () => {
  const caller = getCaller();

  // Define a test file containing audio file with metadata.
  const id3Audio = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'id3Audio.mp3',
    targetPath: 'id3Audio.mp3',
    mimeType: 'audio/mpeg',
    lastModifiedTime: 'December 25 2015, 11:16 PM',
    nameText: 'id3Audio.mp3',
    sizeText: '5KB',
    typeText: 'id3 encoded MP3 audio',
  });

  /**
   * The preview resides in the <files-safe-media> shadow DOM, which
   * is a child of the #quick-view shadow DOM.
   */
  const albumArtWebView = ['#quick-view', '#audio-artwork', previewTag];

  // Open Files app on Downloads containing the audio test file.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [id3Audio], []);

  // Open the file in Quick View.
  await openQuickView(appId, id3Audio.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Wait until the preview has loaded the album image of the audio file.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [albumArtWebView, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('audio/mpeg', mimeType);

  // Check: the audio album metadata should also be displayed.
  const album = await getQuickViewMetadataBoxField(appId, 'Album');
  chrome.test.assertEq(album, 'OK Computer');
};

/**
 * Tests opening Quick View containing an image with extension 'jpg'.
 */
testcase.openQuickViewImageJpg = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.smallJpeg.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallJpeg], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.smallJpeg.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);
};

/**
 * Tests opening Quick View containing an image with extension 'jpeg'.
 */
testcase.openQuickViewImageJpeg = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.sampleJpeg.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.sampleJpeg], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.sampleJpeg.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);
};

/**
 * Tests that opening Quick View on a JPEG image with EXIF displays the EXIF
 * information in the QuickView Metadata Box.
 */
testcase.openQuickViewImageExif = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.exifImage.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.exifImage], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.exifImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);

  // Check: the correct file modified time should be displayed.
  const time = await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq('Jan 18, 2038, 1:02 AM', time);

  // Check: the correct image EXIF metadata should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('378 x 272', size);
  const model = await getQuickViewMetadataBoxField(appId, 'Device model');
  chrome.test.assertEq('FinePix S5000', model);
  const film = await getQuickViewMetadataBoxField(appId, 'Device settings');
  chrome.test.assertEq('f/2.8 0.004 5.7mm ISO200', film);
};

/**
 * Tests opening Quick View on an RAW image. The RAW image has EXIF and that
 * information should be displayed in the QuickView metadata box.
 */
testcase.openQuickViewImageRaw = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.rawImage.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.rawImage], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.rawImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/x-olympus-orf', mimeType);

  // Check: the RAW image EXIF metadata should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('4608 x 3456', size);
  const model = await getQuickViewMetadataBoxField(appId, 'Device model');
  chrome.test.assertEq('E-M1', model);
  const film = await getQuickViewMetadataBoxField(appId, 'Device settings');
  chrome.test.assertEq('f/8 0.002 12mm ISO200', film);
};

/**
 * Tests opening Quick View on an RAW .NEF image and that the dimensions
 * shown in the metadata box respect the image EXIF orientation.
 */
testcase.openQuickViewImageRawWithOrientation = async () => {
  const caller = getCaller();

  /**
   * The <files-safe-media type="image"> element is a shadow DOM child of
   * the #quick-view element, and has a shadow DOM child <webview> or <iframe>
   * for the preview.
   */
  const filesSafeMedia = ['#quick-view', 'files-safe-media[type="image"]'];

  // Open Files app on Downloads containing ENTRIES.rawNef.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.nefImage], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.nefImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    const preview = filesSafeMedia.concat([previewTag]);
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct image dimensions should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('1324 x 4028', size);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/tiff', mimeType);

  // Get the fileSafeMedia element preview thumbnail image size.
  const element = await remoteCall.waitForElement(appId, filesSafeMedia);
  const image = new Image();
  image.onload = () => {
    image.imageSize = `${image.naturalWidth} x ${image.naturalHeight}`;
  };

  const sourceContent =
      /** @type {FilePreviewContent} */ (JSON.parse(element.attributes.src));
  assert(sourceContent.data);
  image.src = sourceContent.data;

  // Check: the preview thumbnail should have an orientiated size.
  await repeatUntil(async () => {
    if (!image.complete || image.imageSize !== '120 x 160') {
      return pending(caller, 'Waiting for preview thumbnail size.');
    }
  });
};

/**
 * Tests opening Quick View with a VP8X format WEBP image.
 */
testcase.openQuickViewImageWebp = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.rawImage.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.webpImage], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.webpImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/webp', mimeType);

  // Check: the correct dimensions should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('400 x 175', size);
};

/**
 * Tests that opening Quick View on an image and clicking the image does not
 * focus the image. Instead, the user should still be able to cycle through
 * file list items in Quick View: crbug.com/1038835.
 */
testcase.openQuickViewImageClick = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing two images.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.desktop, ENTRIES.image3], []);

  // Open the first image in Quick View.
  await openQuickView(appId, ENTRIES.desktop.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  let mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/png', mimeType);

  // Click the image in Quick View to attempt to focus it.
  await remoteCall.waitAndClickElement(appId, preview);

  // Press the down arrow key to attempt to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);

  // Check: Quick View should be able to close.
  await closeQuickView(appId);
};

/**
 * Tests that opening a broken image in Quick View displays the "no-preview
 * available" generic icon and has a [load-error] attribute.
 */
testcase.openQuickViewBrokenImage = async () => {
  const caller = getCaller();

  /**
   * The [generic-thumbnail] element resides in the #quick-view shadow DOM
   * as a sibling of the files-safe-media[type="image"] element.
   */
  const genericThumbnail = [
    '#quick-view',
    'files-safe-media[type="image"][hidden] + [generic-thumbnail="image"]',
  ];

  // Open Files app on Downloads containing ENTRIES.brokenJpeg.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.brokenJpeg], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.brokenJpeg.nameText);

  // Check: the quick view element should have a 'load-error' attribute.
  await remoteCall.waitForElement(appId, '#quick-view[load-error]');

  // Wait for the generic thumbnail to load and display its content.
  function checkForGenericThumbnail(elements) {
    const haveElements = Array.isArray(elements) && elements.length === 1;
    if (!haveElements || elements[0].styles.display !== 'block') {
      return pending(caller, 'Waiting for generic thumbnail to load.');
    }
    return;
  }

  // Check: the generic thumbnail icon should be displayed.
  await repeatUntil(async () => {
    return checkForGenericThumbnail(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [genericThumbnail, ['display']]));
  });
};

/**
 * Tests opening Quick View containing a video.
 */
testcase.openQuickViewVideo = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="video"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.webm], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.webm.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewVideoLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/webm', mimeType);

  // Close Quick View.
  await closeQuickView(appId);

  // Check: closing Quick View should remove the video <files-safe-media>
  // preview element, so it stops playing the video. crbug.com/970192
  await remoteCall.waitForElementLost(appId, preview);
};

/**
 * Tests opening Quick View containing a video on Drive.
 */
testcase.openQuickViewVideoOnDrive = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="video"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.webm]);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.webm.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewVideoLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag(
      appId, preview, getBackgroundStyle);

  if (await isDarkModeEnabled()) {
    // Check: the preview body backgroundColor should be black.
    chrome.test.assertEq('rgb(0, 0, 0)', backgroundColor[0]);
  } else {
    // Check: the preview body backgroundColor should be transparent black.
    chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
  }

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/webm', mimeType);

  // Close Quick View.
  await closeQuickView(appId);

  // Check: closing Quick View should remove the video <files-safe-media>
  // preview element, so it stops playing the video. crbug.com/970192
  await remoteCall.waitForElementLost(appId, preview);
};

/**
 * Tests opening Quick View with multiple files and using the up/down arrow
 * keys to select and view their content.
 */
testcase.openQuickViewKeyboardUpDownChangesView = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing two text files.
  const files = [ENTRIES.hello, ENTRIES.tallText];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Open the last file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });

  // Press the up arrow key to select the previous file.
  const upArrow = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, upArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });
};

/**
 * Tests opening Quick View with multiple files and using the left/right arrow
 * keys to select and view their content.
 */
testcase.openQuickViewKeyboardLeftRightChangesView = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing two text files.
  const files = [ENTRIES.hello, ENTRIES.tallText];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Open the last file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the right arrow key to select the next file item.
  const rightArrow = ['#quick-view', 'ArrowRight', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, rightArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });

  // Press the left arrow key to select the previous file item.
  const leftArrow = ['#quick-view', 'ArrowLeft', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, leftArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });
};

/**
 * Tests that the metadatabox can be toggled opened/closed by pressing the
 * Enter key on the Quick View toolbar info button.
 */
testcase.openQuickViewToggleInfoButtonKeyboard = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check: the metadatabox should be open.
  const metaShown = ['#quick-view', '#contentPanel[metadata-box-active]'];
  await remoteCall.waitForElement(appId, metaShown);

  // The toolbar info button query differs in files-ng.
  const quickView = await remoteCall.waitForElement(appId, ['#quick-view']);
  let infoButton = ['#quick-view', '#metadata-button'];
  if (quickView.attributes['files-ng'] !== undefined) {
    infoButton = ['#quick-view', '#info-button'];
  }

  // Press Enter key on the info button.
  const key = [infoButton, 'Enter', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Check: the metadatabox should close.
  await remoteCall.waitForElementLost(appId, metaShown);

  // Press Enter key on the info button.
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Check: the metadatabox should open.
  await remoteCall.waitForElement(appId, metaShown);
};

/**
 * Tests that the metadatabox can be toggled opened/closed by clicking the
 * the Quick View toolbar info button.
 */
testcase.openQuickViewToggleInfoButtonClick = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check: the metadatabox should be open.
  const metaShown = ['#quick-view', '#contentPanel[metadata-box-active]'];
  await remoteCall.waitForElement(appId, metaShown);

  // The toolbar info button query differs in files-ng.
  const quickView = await remoteCall.waitForElement(appId, ['#quick-view']);
  let infoButton = ['#quick-view', '#metadata-button'];
  if (quickView.attributes['files-ng'] !== undefined) {
    infoButton = ['#quick-view', '#info-button'];
  }

  // Click the info button.
  await remoteCall.waitAndClickElement(appId, infoButton);

  // Check: the metadatabox should close.
  await remoteCall.waitForElementLost(appId, metaShown);

  // Click the info button.
  await remoteCall.waitAndClickElement(appId, infoButton);

  // Check: the metadatabox should open.
  await remoteCall.waitForElement(appId, metaShown);
};

/**
 * Tests that Quick View opens with multiple files selected.
 */
testcase.openQuickViewWithMultipleFiles = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Add item 3 to the check-selection, ENTRIES.desktop.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  for (let i = 0; i < 3; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
        'ArrowDown failed');
  }
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 5 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['Desktop', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: ENTRIES.desktop should be displayed in the preview.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct file mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/png', mimeType);
};

/**
 * Tests that Quick View displays text files when multiple files are
 * selected.
 */
testcase.openQuickViewWithMultipleFilesText = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  const files = [ENTRIES.tallText, ENTRIES.hello, ENTRIES.smallJpeg];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.smallJpeg.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['small', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the image file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const textView = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the text file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [textView, ['display']]));
  });

  // Check: the open button should be displayed.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
};

/**
 * Tests that Quick View displays pdf files when multiple files are
 * selected.
 */
testcase.openQuickViewWithMultipleFilesPdf = async () => {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  const files = [ENTRIES.tallPdf, ENTRIES.desktop, ENTRIES.smallJpeg];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.smallJpeg.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.tallPdf.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['small', 'tall']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the image file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const pdfView = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the pdf file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [pdfView, ['display']]));
  });

  // Check: the open button should be displayed.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
};

/**
 * Tests that the content panel changes when using the up/down arrow keys
 * when multiple files are selected.
 */
testcase.openQuickViewWithMultipleFilesKeyboardUpDown = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing three text files.
  const files = [ENTRIES.hello, ENTRIES.tallText, ENTRIES.plainText];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.tallText.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['tall', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    // Check: the content of ENTRIES.hello should be shown.
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });

  // Press the up arrow key to select the previous file.
  const upArrow = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, upArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    // Check: the content of ENTRIES.tallText should be shown.
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });
};

/**
 * Tests that the content panel changes when using the left/right arrow keys
 * when multiple files are selected.
 */
testcase.openQuickViewWithMultipleFilesKeyboardLeftRight = async () => {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing three text files.
  const files = [ENTRIES.hello, ENTRIES.tallText, ENTRIES.plainText];
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.tallText.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['tall', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || !elements[0].attributes.src) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the right arrow key to select the next file item.
  const rightArrow = ['#quick-view', 'ArrowRight', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, rightArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {

    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    // Check: the content of ENTRIES.hello should be shown.
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });

  // Press the left arrow key to select the previous file item.
  const leftArrow = ['#quick-view', 'ArrowLeft', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, leftArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
        appId, preview, getTextContent);
    // Check: the content of ENTRIES.tallText should be shown.
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
  });
};

/**
 * Tests opening Quick View and closing with Escape key returns focus to file
 * list.
 */
testcase.openQuickViewAndEscape = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Hit Escape key to close Quick View.
  const panelElements = ['#quick-view', '#contentPanel'];
  const key = [panelElements, 'Escape', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown Escape failed');

  // Check: the Quick View element should not be shown.
  await waitQuickViewClose(appId);

  // Check: the file list should gain the focus.
  const element = await remoteCall.waitForElement(appId, '#file-list:focus');
  chrome.test.assertEq(
      'file-list', element.attributes['id'], '#file-list should be focused');
};

/**
 * Test opening Quick View when Directory Tree is focused it should display if
 * there is only 1 file/folder selected in the file list.
 */
testcase.openQuickViewFromDirectoryTree = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Focus Directory Tree.
  await remoteCall.focus(appId, ['#directory-tree']);

  // Ctrl+A to select the only file.
  const ctrlA = ['#directory-tree', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Use selection menu button to open Quick View.
  await remoteCall.simulateUiClick(
      appId, '#selection-menu-button:not([hidden])');

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the Menu item to show the Quick View.
  const getInfoMenuItem = '#file-context-menu:not([hidden]) ' +
      ' [command="#get-info"]:not([hidden])';
  await remoteCall.simulateUiClick(appId, getInfoMenuItem);

  // Check: the Quick View dialog should be shown.
  const caller = getCaller();
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });
};

/**
 * Tests the tab-index focus order when sending tab keys when an image file is
 * shown in Quick View.
 */
testcase.openQuickViewTabIndexImage = async () => {
  // Get tab-index focus query item texts.
  const backText = await i18nQuickViewLabelText('Back');
  const openText = await i18nQuickViewLabelText('Open');
  const deleteText = await i18nQuickViewLabelText('Delete');
  const fileInfoText = await i18nQuickViewLabelText('File info');

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${openText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${deleteText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${fileInfoText}"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.smallJpeg.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallJpeg], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.smallJpeg.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
};

/**
 * Tests the tab-index focus order when sending tab keys when a text file is
 * shown in Quick View.
 */
testcase.openQuickViewTabIndexText = async () => {
  // Get tab-index focus query item texts.
  const backText = await i18nQuickViewLabelText('Back');
  const openText = await i18nQuickViewLabelText('Open');
  const deleteText = await i18nQuickViewLabelText('Delete');
  const fileInfoText = await i18nQuickViewLabelText('File info');

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${openText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${deleteText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${fileInfoText}"]:focus`]},
    {'query': ['#quick-view']},  // Tab past the content panel.
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.tallText.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
};

/**
 * Tests the tab-index focus order when sending tab keys when an HTML file is
 * shown in Quick View.
 */
testcase.openQuickViewTabIndexHtml = async () => {
  // Get tab-index focus query item texts.
  const backText = await i18nQuickViewLabelText('Back');
  const openText = await i18nQuickViewLabelText('Open');
  const deleteText = await i18nQuickViewLabelText('Delete');
  const fileInfoText = await i18nQuickViewLabelText('File info');

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${openText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${deleteText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${fileInfoText}"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallHtml.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
};

/**
 * Tests the tab-index focus order when sending tab keys when an audio file
 * is shown in Quick View.
 */
testcase.openQuickViewTabIndexAudio = async () => {
  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.beautiful.nameText);

  // Get tab-index focus query item texts.
  const backText = await i18nQuickViewLabelText('Back');
  const openText = await i18nQuickViewLabelText('Open');
  const deleteText = await i18nQuickViewLabelText('Delete');
  const fileInfoText = await i18nQuickViewLabelText('File info');

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${openText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${deleteText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${fileInfoText}"]:focus`]},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }

  // Send tab keys until Back gains the focus again.
  while (true) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: back should eventually get the focus again.
    const activeElement =
        await remoteCall.callRemoteTestUtil('deepGetActiveElement', appId, []);
    if (activeElement.attributes['aria-label'] === backText) {
      break;
    }
  }
};

/**
 * Tests the tab-index focus order when sending tab keys when a video file is
 * shown in Quick View.
 */
testcase.openQuickViewTabIndexVideo = async () => {
  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.webm], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.webm.nameText);

  // Get tab-index focus query item texts.
  const backText = await i18nQuickViewLabelText('Back');
  const openText = await i18nQuickViewLabelText('Open');
  const deleteText = await i18nQuickViewLabelText('Delete');
  const fileInfoText = await i18nQuickViewLabelText('File info');

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="${backText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${openText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${deleteText}"]:focus`]},
    {'query': ['#quick-view', `[aria-label="${fileInfoText}"]:focus`]},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }

  // Send tab keys until Back gains the focus again.
  while (true) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: back should eventually get the focus again.
    const activeElement =
        await remoteCall.callRemoteTestUtil('deepGetActiveElement', appId, []);
    if (activeElement.attributes['aria-label'] === backText) {
      break;
    }
  }
};

/**
 * Tests that the tab-index focus stays within the delete confirm dialog.
 */
testcase.openQuickViewTabIndexDeleteDialog = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a USB file in Quick View. USB delete never uses trash and always
  // shows the delete dialog.
  await mountAndSelectUsb(appId);
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Open the Quick View delete confirm dialog.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: the Quick View delete confirm dialog should open.
  await remoteCall.waitForElement(
      appId,  // The <cr-dialog> is a child of the Quick View shadow DOM.
      ['#quick-view', '.cr-dialog-container.shown .cr-dialog-cancel:focus']);

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', '.cr-dialog-ok:not([hidden])']},
    {'query': ['#quick-view', '.cr-dialog-cancel:not([hidden])']},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', shift: query.shift || false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
};

/**
 * Tests deleting an item from Quick View when in single select mode, and
 * that Quick View closes when there are no more items to view.
 */
testcase.openQuickViewAndDeleteSingleSelection = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Press delete key.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  if (await sendTestMessage({name: 'isTrashEnabled'}) !== 'true') {
    // Check: the delete confirm dialog should focus the 'Cancel' button.
    let defaultDialogButton = ['#quick-view', '.cr-dialog-cancel:focus'];
    defaultDialogButton =
        await remoteCall.waitForElement(appId, defaultDialogButton);
    chrome.test.assertEq('Cancel', defaultDialogButton.text);

    // Click the delete confirm dialog 'Delete' button.
    let deleteDialogButton = ['#quick-view', '.cr-dialog-ok:not([hidden])'];
    deleteDialogButton =
        await remoteCall.waitAndClickElement(appId, deleteDialogButton);
    chrome.test.assertEq('Delete forever', deleteDialogButton.text);
  }

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
};

/**
 * Tests deleting an item from Quick View while in check-selection mode.
 * Deletes the item at the bottom of the file list, and checks that
 * the item below the item deleted is shown in Quick View after the item's
 * deletion.
 */
testcase.openQuickViewAndDeleteCheckSelection = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  const caller = getCaller();

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA),
      'Ctrl+A failed');

  // Open Quick View via its keyboard shortcut.
  const space = ['#file-list', ' ', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);

  // Press the up arrow to go to the last file in the selection.
  const quickViewArrowUp = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, quickViewArrowUp));

  // Press delete key.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  if (await sendTestMessage({name: 'isTrashEnabled'}) !== 'true') {
    // Check: the delete confirm dialog should focus the 'Cancel' button.
    let defaultDialogButton = ['#quick-view', '.cr-dialog-cancel:focus'];
    defaultDialogButton =
        await remoteCall.waitForElement(appId, defaultDialogButton);
    chrome.test.assertEq('Cancel', defaultDialogButton.text);

    // Click the delete confirm dialog 'Delete' button.
    let deleteDialogButton = ['#quick-view', '.cr-dialog-ok:not([hidden])'];
    deleteDialogButton =
        await remoteCall.waitAndClickElement(appId, deleteDialogButton);
    chrome.test.assertEq('Delete forever', deleteDialogButton.text);
  }

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: Quick View should display the entry below |hello.txt|,
  // which is |world.ogv|.
  function checkPreviewVideoLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  const videoWebView =
      ['#quick-view', 'files-safe-media[type="video"]', previewTag];
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [videoWebView, ['display']]));
  });

  // Check: the mimeType of |world.ogv| should be 'video/ogg'.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/ogg', mimeType);
};

/**
 * Tests that deleting all items in a check-selection closes the Quick View.
 */
testcase.openQuickViewDeleteEntireCheckSelection = async () => {
  const caller = getCaller();

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Check-select Beautiful Song.ogg and My Desktop Background.png.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Pressing Ctrl+Space failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Pressing Ctrl+Space failed.');

  // Open Quick View on the check-selected files.
  await openQuickViewMultipleSelection(appId, ['Beautiful', 'Desktop']);

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const audioWebView =
      ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [audioWebView, ['display']]));
  });

  // Press delete.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Click the delete confirm dialog OK button.
  const deleteConfirm = ['#quick-view', '.cr-dialog-ok:not([hidden])'];
  if (await sendTestMessage({name: 'isTrashEnabled'}) !== 'true') {
    await remoteCall.waitAndClickElement(appId, deleteConfirm);
  }

  // Check: |Beautiful Song.ogg| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="Beautiful Song.ogg"]');

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const imageWebView =
      ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0].styles.display.includes('block');
    }
    if (!haveElements || elements[0].attributes.loaded !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [imageWebView, ['display']]));
  });

  // Press delete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Click the delete confirm dialog OK button.
  if (await sendTestMessage({name: 'isTrashEnabled'}) !== 'true') {
    await remoteCall.waitAndClickElement(appId, deleteConfirm);
  }

  // Check: |My Desktop Background.png| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="My Desktop Background.png"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
};

/**
 * Tests that an item can be deleted using the Quick View delete button.
 */
testcase.openQuickViewClickDeleteButton = async () => {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Click the Quick View delete button.
  const quickViewDeleteButton = ['#quick-view', '#delete-button:not([hidden])'];
  await remoteCall.waitAndClickElement(appId, quickViewDeleteButton);

  // Click the delete confirm dialog OK button.
  if (await sendTestMessage({name: 'isTrashEnabled'}) !== 'true') {
    const deleteConfirm = ['#quick-view', '.cr-dialog-ok:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, deleteConfirm);
  }

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
};

/**
 * Tests that the delete button is not shown if the file displayed in Quick
 * View cannot be deleted.
 */
testcase.openQuickViewDeleteButtonNotShown = async () => {
  // Open Files app on My Files
  const appId = await openNewWindow('');

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: My Files should contain the expected entries.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Open Play files in Quick View, which cannot be deleted.
  await openQuickView(appId, 'Play files');

  // Check: the delete button should not be shown.
  const quickViewDeleteButton = ['#quick-view', '#delete-button[hidden]'];
  await remoteCall.waitForElement(appId, quickViewDeleteButton);
};

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when opening
 * a single file via Quick View using "Get Info" from the context menu.
 */
testcase.openQuickViewUmaViaContextMenu = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Open Quick View via the entry context menu.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening + 1);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening, selectionMenuUMAValueBeforeOpening);
};

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the context
 * menu.
 */
testcase.openQuickViewUmaForCheckSelectViaContextMenu = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Open Quick View using the context menu.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening + 1);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening, selectionMenuUMAValueBeforeOpening);
};

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the Selection
 * menu.
 */
testcase.openQuickViewUmaViaSelectionMenu = async () => {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  const caller = getCaller();

  // Wait until the selection menu is visible.
  function checkElementsDisplayVisible(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length == 0 || elements[0].styles.display === 'none') {
      return pending(caller, 'Waiting for Selection Menu to be visible.');
    }
  }

  await repeatUntil(async () => {
    const elements = ['#selection-menu-button'];
    return checkElementsDisplayVisible(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [elements, ['display']]));
  });

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Click the Selection Menu button. Using fakeMouseClick causes
  // the focus to switch from file-list such that crbug.com/1046997
  // cannot be tested, use simulateUiClick() instead.
  await remoteCall.simulateUiClick(
      appId, '#selection-menu-button:not([hidden])');

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the file-list context menu "Get info" command.
  await remoteCall.simulateUiClick(
      appId,
      '#file-context-menu:not([hidden]) [command="#get-info"]:not([hidden])');

  // Check: the Quick View dialog should be shown.
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening,
      selectionMenuUMAValueBeforeOpening + 1);
};

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the context
 * menu opened via keyboard tabbing (not mouse).
 */
testcase.openQuickViewUmaViaSelectionMenuKeyboard = async () => {
  const caller = getCaller();

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Wait until the selection menu is visible.
  function checkElementsDisplayVisible(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length == 0 || elements[0].styles.display === 'none') {
      return pending(caller, 'Waiting for Selection Menu to be visible.');
    }
  }

  await repeatUntil(async () => {
    const elements = ['#selection-menu-button'];
    return checkElementsDisplayVisible(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [elements, ['display']]));
  });

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Tab to the Selection Menu button.
  await repeatUntil(async () => {
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    const element =
        await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);

    if (element && element.attributes['id'] === 'selection-menu-button') {
      return true;
    }
    return pending(
        caller, 'Waiting for selection-menu-button to become active');
  });

  // Key down to the "Get Info" command.
  await repeatUntil(async () => {
    const keyDown =
        ['#selection-menu-button', 'ArrowDown', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, keyDown));

    const element =
        await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);

    if (element && element.attributes['command'] === '#get-info') {
      return true;
    }
    return pending(caller, 'Waiting for get-info command to become active');
  });

  // Select the "Get Info" command using the Enter key.
  const keyEnter = ['#selection-menu-button', 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, keyEnter));

  // Check: the Quick View dialog should be shown.
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening,
      selectionMenuUMAValueBeforeOpening + 1);
};
