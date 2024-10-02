// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, EntryType, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openNewWindow, remoteCall} from './background.js';

/**
 * Info for the source or destination of a transfer.
 */
class TransferLocationInfo {
  /*
   * Create a new TransferLocationInfo.
   *
   * @param{{
         volumeName: !string,
         breadcrumbsPath: !string,
         enterpriseConnectorsVolumeIdentifier: !string,
     }} opts Options for creating TransferLocationInfo.
   */
  constructor(opts) {
    /**
     * The volume type (e.g. downloads, drive, drive_recent,
     * drive_shared_with_me, drive_offline) or team drive name.
     * @type {string}
     */
    this.volumeName = opts.volumeName;

    /** @type {string} */
    this.breadcrumbsPath = opts.breadcrumbsPath;

    /**
     * Identifies the volume and can be used for the
     * OnFileTransferEnterpriseConnector policy. This should match the allowed
     * policy values in policy_templates.json or
     * source_destination_matcher_ash.cc.
     * @type {string}
     */
    this.enterpriseConnectorsVolumeIdentifier =
        opts.enterpriseConnectorsVolumeIdentifier;
  }
}

/**
 * Info for the transfer operation.
 */
class TransferInfo {
  /*
   * Create a new TransferInfo.
   *
   * @param{{
         source: !TransferLocationInfo,
         destination: !TransferLocationInfo,
         expectedDialogText: string,
         isMove: boolean,
         expectFailure: boolean,
     }} opts Options for creating TransferInfo.
   */
  constructor(opts) {
    /**
     * The source location.
     * @type {!TransferLocationInfo}
     */
    this.source = opts.source;

    /**
     * The destination location.
     * @type {!TransferLocationInfo}
     */
    this.destination = opts.destination;

    /**
     * True if this transfer is for a move operation, false for a copy
     * operation.
     * @type {!boolean}
     */
    this.isMove = opts.isMove || false;

    /**
     * Whether the test is expected to fail, i.e. transferring blocked files.
     * @type {!boolean}
     */
    this.expectFailure = opts.expectFailure || false;
  }
}

/**
 * Flat connector entry test set that does not include any directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file is allowed, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_FLAT = [
  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'a_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'a_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'b_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'b_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'c_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'c_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * Test set to test deep scanninng, contains nested directories.
 *
 * If a file should be blocked, name it "*blocked*".
 * If a file is allowed, name it "*allowed*".
 * If a directory only contains allowed files, name it "*allowed*".
 */
const CONNECTOR_ENTRIES_DEEP = [
  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B_allowed',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'B_allowed',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'C',
    sizeText: '--',
    typeText: 'Folder',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B_allowed/g_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'g_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/i_blocked.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'i_blocked.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/C/j_allowed.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'j_allowed.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),
];

/**
 * A list of transfer locations, for use with transferBetweenVolumes.
 * volumeName has to match an entry of
 * AddEntriesMessage::MapStringToTargetVolume().
 * @enum {TransferLocationInfo}
 */
const TRANSFER_LOCATIONS = {
  drive: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    enterpriseConnectorsVolumeIdentifier: 'GOOGLE_DRIVE',
  }),

  downloads: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Downloads',
    volumeName: 'local',
    enterpriseConnectorsVolumeIdentifier: 'MY_FILES',
  }),

  crostini: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Linux files',
    volumeName: 'crostini',
    enterpriseConnectorsVolumeIdentifier: 'CROSTINI',
  }),

  usb: new TransferLocationInfo({
    breadcrumbsPath: '/fake-usb',
    volumeName: 'usb',
    enterpriseConnectorsVolumeIdentifier: 'REMOVABLE',
  }),

  smbfs: new TransferLocationInfo({
    breadcrumbsPath: '/SMB Share',
    volumeName: 'smbfs',
    enterpriseConnectorsVolumeIdentifier: 'SMB',
  }),

  mtp: new TransferLocationInfo({
    breadcrumbsPath: '/fake-mtp',
    volumeName: 'mtp',
    enterpriseConnectorsVolumeIdentifier: 'DEVICE_MEDIA_STORAGE',
  }),

  android_files: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Play files',
    volumeName: 'android_files',
    enterpriseConnectorsVolumeIdentifier: 'ARC',
  }),
};
Object.freeze(TRANSFER_LOCATIONS);

// TODO(crbug.com/1361898): Remove these ones proper error details are
// displayed.
const COPY_FAIL_MESSAGE =
    'Copy operation failed. The file could not be accessed ' +
    'for security reasons.';
const MOVE_FAIL_DIRECTORY_MESSAGE =
    `Move failed. The file could not be modified.`;
const MOVE_FAIL_FILE_MESSAGE = 'Move failed. The file could not be accessed ' +
    'for security reasons.';

/**
 * Opens a Files app's main window and creates the source and destination
 * entries.
 * @param {!TransferInfo} transferInfo Options for the transfer.
 * @return {Promise} Promise to be fulfilled with the window ID.
 */
async function setupForFileTransferConnector(
    transferInfo, srcContents, dstContents) {
  const sourceEntriesPromise =
      addEntries([transferInfo.source.volumeName], srcContents);
  const destEntriesPromise =
      addEntries([transferInfo.destination.volumeName], dstContents);

  const appId = await openNewWindow(RootPath.DOWNLOADS, {});
  await remoteCall.waitForElement(appId, '#detail-table');

  // Wait until the elements are loaded in the table.
  await Promise.all([
    remoteCall.waitForFileListChange(appId, 0),
    sourceEntriesPromise,
    destEntriesPromise,
  ]);
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
  return appId;
}

/**
 * Returns all entries that are children of the passed directory.
 * @param {!Array<!TestEntryInfo>} entries The entries.
 * @param {!Array<string>} directory The directory path.
 *  Contains the path of the current directory, e.g., ["A", "B"] for A/B/.
 */
function getCurrentEntries(entries, directory) {
  return entries.filter(entry => {
    const parent = entry.targetPath.split('/').slice(0, -1);
    return parent.length == directory.length &&
        parent.every((value, index) => value === directory[index]);
  });
}

/**
 * Verifies the recursive contents of the current path by checking the file list
 * of the current path and its ancestors.
 * @param {string} appId App window Id.
 * @param {!Array<!TestEntryInfo>} expectedEntries Expected contents of file
 *     list.
 * @param {string} rootDirectory The path to the root directory for the check.
 * @param {!Array<string>} currentSubDirectory The current directory path split
 *     at '/', e.g., ["A", "B"] for A/B/.
 */
async function verifyDirectoryRecursively(
    appId, expectedEntries, rootDirectory, currentSubDirectory = []) {
  // 1. Check current directory.
  const currentEntries =
      getCurrentEntries(expectedEntries, currentSubDirectory);
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(currentEntries),
      {ignoreLastModifiedTime: true});

  // 2. For each subdirectory: enter subdirectory and call recursion.
  for (const entry of currentEntries.filter(
           entry => entry.type === EntryType.DIRECTORY)) {
    currentSubDirectory.push(entry.nameText);
    await navigateWithDirectoryTree(
        appId, rootDirectory + '/' + currentSubDirectory.join('/'));
    await verifyDirectoryRecursively(
        appId, expectedEntries, rootDirectory, currentSubDirectory);
    currentSubDirectory.pop();
  }

  // 3. After the recursion ends, navigate back to the root directory.
  if (currentSubDirectory.length == 0) {
    // Go back to the root directory.
    await navigateWithDirectoryTree(appId, rootDirectory);
  }
}

/**
 * Function to toggle display of all play files.
 * Before this function is called, the play file folder has to be opened.
 * @param {string} appId App window Id.
 */
async function showAllPlayFiles(appId) {
  const toggleMenuItemSelector = '#gear-menu-toggle-hidden-android-folders';

  // Open the gear menu by clicking the gear button.
  await remoteCall.waitAndClickElement(appId, '#gear-button:not([hidden])');

  // Wait for the gear-menu to appear and click the menu item.
  await remoteCall.waitAndClickElement(
      appId,
      `#gear-menu:not([hidden]) ${
          toggleMenuItemSelector}:not([disabled]):not([checked])`);

  // Wait for item to be checked.
  await remoteCall.waitForElement(appId, toggleMenuItemSelector + '[checked]');
}

/**
 * Test function to copy from the specified source to the specified destination.
 * @param {!TransferInfo} transferInfo Options for the transfer.
 * @param {!Array<!TestEntryInfo>} entryTestSet The set of file and directory
 *     entries to be used for the test.
 * @param {string} expectedFinalMsg The final message to expect at the progress
 *     center.
 */
async function transferBetweenVolumes(
    transferInfo, entryTestSet, expectedFinalMsg) {
  // Setup volumes
  if (transferInfo.source.volumeName === 'usb' ||
      transferInfo.destination.volumeName === 'usb') {
    await sendTestMessage({name: 'mountFakeUsbEmpty'});
  }
  if (transferInfo.source.volumeName === 'smbfs' ||
      transferInfo.destination.volumeName === 'smbfs') {
    await sendTestMessage({name: 'mountSmbfs'});
  }
  if (transferInfo.source.volumeName === 'mtp' ||
      transferInfo.destination.volumeName === 'mtp') {
    await sendTestMessage({name: 'mountFakeMtpEmpty'});
  }

  // Setup policy.
  await sendTestMessage({
    name: 'setupFileTransferPolicy',
    source: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
    destination: transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
  });

  // Setup reporting expectations.
  await sendTestMessage({
    name: 'expectFileTransferReports',
    source_volume: transferInfo.source.enterpriseConnectorsVolumeIdentifier,
    destination_volume:
        transferInfo.destination.enterpriseConnectorsVolumeIdentifier,
    entry_paths: entryTestSet.filter(entry => entry.type === EntryType.FILE)
                     .map(entry => entry.targetPath),
  });

  // Setup the scanning closure to be able to wait for the scanning to be
  // complete.
  await sendTestMessage({
    name: 'setupScanningRunLoop',
    number_of_expected_delegates:
        entryTestSet.filter(entry => !entry.targetPath.includes('/')).length,
  });

  const dstContents = TestEntryInfo.getExpectedRows([ENTRIES.hello]);

  // Opens Files app and initiates source with entryTestSet and destination
  // with [ENTRIES.hello].
  // The destination is populated to prevent flakes (we can wait for the `hello`
  // file to appear).
  const appId = await setupForFileTransferConnector(
      transferInfo, entryTestSet, [ENTRIES.hello]);

  // Select the source folder.
  await navigateWithDirectoryTree(appId, transferInfo.source.breadcrumbsPath);

  if (transferInfo.source.volumeName === 'android_files') {
    await showAllPlayFiles(appId);
  }

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(getCurrentEntries(entryTestSet, [])),
      {ignoreLastModifiedTime: true});

  // Focus the file list.
  await remoteCall.focus(appId, ['#file-list:not([hidden])']);

  // Select all files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);
  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  // Copy the files. Similar to (ctrl + c) or (ctrl + x).
  // The actual copy only starts with the paste (ctrl + v).
  const transferCommand = transferInfo.isMove ? 'cut' : 'copy';
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'execCommand', appId, [transferCommand]));

  // Select the destination folder.
  await navigateWithDirectoryTree(
      appId, transferInfo.destination.breadcrumbsPath);

  // Wait for the initially expected files to appear in the file list.
  // This is before the actual copy!
  await remoteCall.waitForFiles(
      appId, dstContents, {ignoreFileSize: true, ignoreLastModifiedTime: true});
  // Paste the file. Similar to (ctrl + v).
  // This will execute the actual paste.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  const reportOnly =
      await sendTestMessage({name: 'isReportOnlyFileTransferConnector'}) ===
      'true';
  if (reportOnly) {
    await verifyAfterPasteReportOnly(appId, transferInfo, entryTestSet);
  } else {
    await verifyAfterPasteBlocking(
        appId, transferInfo, entryTestSet, expectedFinalMsg);
  }
}

/**
 * Verify what happens after a paste when scanning can block files.
 *
 * @param {string} appId The app id of the files app window.
 * @param {!TransferInfo} transferInfo Options for the transfer.
 * @param {!Array<!TestEntryInfo>} entryTestSet The set of file and directory
 *     entries to be used for the test.
 * @param {string} expectedFinalMsg The final message to expect at the progress
 *     center.
 */
async function verifyAfterPasteBlocking(
    appId, transferInfo, entryTestSet, expectedFinalMsg) {
  // Check that a scanning label is shown.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);

    const actualPrimaryText = element.attributes['primary-text'];
    const actualSecondaryText = element.attributes['secondary-text'];

    const expectedPrimaryTextPart = transferInfo.isMove ? 'Moving' : 'Copying';
    const expectedSecondaryText = 'Scanning';
    if (actualPrimaryText.includes(expectedPrimaryTextPart) &&
        actualSecondaryText === expectedSecondaryText) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedPrimaryTextPart}... - ${
            expectedSecondaryText}", got "${actualPrimaryText} - ${
            actualSecondaryText}"`);
  });

  // After the scanning label is shown, we resume the transfer.
  // Issue the responses, s.t., the transfer can continue.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  // Wait for the expected files to appear in the file list.
  // Files marked as 'blocked' should not appear.
  const expectedEntries =
      entryTestSet.concat([ENTRIES.hello])
          .filter(entry => !entry.targetPath.includes('blocked'));
  await verifyDirectoryRecursively(
      appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

  // Verify contents of the source directory.
  await navigateWithDirectoryTree(appId, transferInfo.source.breadcrumbsPath);
  let expectedSourceEntries = entryTestSet;
  if (transferInfo.isMove) {
    // For a move, paths that include "allowed" should not be present at the
    // source.
    expectedSourceEntries = expectedSourceEntries.filter(
        entry => !entry.targetPath.includes('allowed'));
  }
  // Wait for the expected files to appear in the file list.
  await verifyDirectoryRecursively(
      appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

  // Check that the error appears in the feedback panel.
  // TODO(crbug.com/1361898): Adapt this check for proper error details.
  let element = {};
  await repeatUntil(async () => {
    element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);

    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedFinalMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedFinalMsg}", got "${
            actualMsg}"`);
  });

  // Check that only one line of text is shown.
  chrome.test.assertFalse(!!element.attributes['secondary-text']);
}

/**
 * Verify what happens after a paste in the case of report-only scans.
 *
 * @param {string} appId The app id of the files app window.
 * @param {!TransferInfo} transferInfo Options for the transfer.
 * @param {!Array<!TestEntryInfo>} entryTestSet The set of file and directory
 *     entries to be used for the test.
 */
async function verifyAfterPasteReportOnly(appId, transferInfo, entryTestSet) {
  // No check for scanning label, as there shouldn't be one.

  // Wait for the expected files to appear in the file list.
  // All files should appear, even those marked as 'blocked'.
  const expectedEntries = entryTestSet.concat([ENTRIES.hello]);
  await verifyDirectoryRecursively(
      appId, expectedEntries, transferInfo.destination.breadcrumbsPath);

  // Verify contents of the source directory.
  await navigateWithDirectoryTree(appId, transferInfo.source.breadcrumbsPath);
  let expectedSourceEntries = entryTestSet;
  if (transferInfo.isMove) {
    // For a move, the source directory should be empty.
    expectedSourceEntries = [];
  }
  // Wait for the expected files to appear in the file list.
  await verifyDirectoryRecursively(
      appId, expectedSourceEntries, transferInfo.source.breadcrumbsPath);

  // Check that the status panel automatically vanishes.
  // This means that there was no error.
  await remoteCall.waitForElementLost(
      appId, ['#progress-panel', 'xf-panel-item']);

  // After the transfer completed, we issue scanning responses.
  // This ensures that scanning does not impact the transfer.
  await sendTestMessage({name: 'issueFileTransferResponses'});

  // We have to wait for the scanning to be completed to fulfill the report
  // expectations.
  await sendTestMessage({name: 'waitForFileTransferScanningToComplete'});
}

/**
 * Tests copying from android_files to Downloads.
 */
testcase.transferConnectorFromAndroidFilesToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.android_files,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromAndroidFilesToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.android_files,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};

/**
 * Tests copying from Crostini to Downloads.
 */
testcase.transferConnectorFromCrostiniToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.crostini,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromCrostiniToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.crostini,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};

/**
 * Tests copying from Drive to Downloads.
 */
testcase.transferConnectorFromDriveToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromDriveToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};

/**
 * Tests moving from Drive to Downloads.
 */
testcase.transferConnectorFromDriveToDownloadsMoveDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_DEEP,
      MOVE_FAIL_DIRECTORY_MESSAGE,
  );
};
testcase.transferConnectorFromDriveToDownloadsMoveFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.drive,
        destination: TRANSFER_LOCATIONS.downloads,
        isMove: true,
      }),
      CONNECTOR_ENTRIES_FLAT,
      MOVE_FAIL_FILE_MESSAGE,
  );
};

/**
 * Tests copying from mtp to Downloads.
 */
testcase.transferConnectorFromMtpToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.mtp,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromMtpToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.mtp,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};

/**
 * Tests copying from smbfs to Downloads.
 */
testcase.transferConnectorFromSmbfsToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.smbfs,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromSmbfsToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.smbfs,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};

/**
 * Tests copying from usb to Downloads.
 */
testcase.transferConnectorFromUsbToDownloadsDeep = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_DEEP,
      COPY_FAIL_MESSAGE,
  );
};
testcase.transferConnectorFromUsbToDownloadsFlat = () => {
  return transferBetweenVolumes(
      new TransferInfo({
        source: TRANSFER_LOCATIONS.usb,
        destination: TRANSFER_LOCATIONS.downloads,
      }),
      CONNECTOR_ENTRIES_FLAT,
      COPY_FAIL_MESSAGE,
  );
};
