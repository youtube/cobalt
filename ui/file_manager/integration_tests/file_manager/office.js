// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, getHistogramCount, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_EXTENSIONS_ID, FILE_MANAGER_SWA_APP_ID, FILE_SWA_BASE_URL} from './test_data.js';

/**
 * Returns 'Open in Google Docs' task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function openDocWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-word`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns 'Open with Excel' task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function openExcelWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-excel`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}

/**
 * Returns 'Open in PowerPoint' task descriptor.
 *
 * @return {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
function openPowerPointWithDriveDescriptor() {
  const filesAppId = FILE_MANAGER_SWA_APP_ID;
  const filesTaskType = 'web';
  const actionId = `${FILE_SWA_BASE_URL}?open-web-drive-office-powerpoint`;

  return {appId: filesAppId, taskType: filesTaskType, actionId: actionId};
}


/**
 * Waits for the expected number of tasks executions, and returns the descriptor
 * of the last executed task.
 *
 * @param {string} appId Window ID.
 * @param {number} expectedCount
 * @return {!Promise<!chrome.fileManagerPrivate.FileTaskDescriptor>}
 */
async function getExecutedTask(appId, expectedCount = 1) {
  const caller = getCaller();

  // Wait until a task has been executed.
  await repeatUntil(async () => {
    const executeTaskCount = await remoteCall.callRemoteTestUtil(
        'staticFakeCounter', appId, ['chrome.fileManagerPrivate.executeTask']);
    if (executeTaskCount == expectedCount) {
      return true;
    }
    chrome.test.assertTrue(executeTaskCount < expectedCount);
    return pending(caller, 'Waiting for a task to execute');
  });

  // Arguments provided for the last call to executeTask().
  const executeTaskArgs = (await remoteCall.callRemoteTestUtil(
      'staticFakeCalledArgs', appId,
      ['chrome.fileManagerPrivate.executeTask']))[expectedCount - 1];

  // The task descriptor is the first argument.
  return executeTaskArgs[0];
}

testcase.openOfficeWordFile = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [ENTRIES.smallDocxHosted.targetPath],
    openType: 'launch',
  });

  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Disable office setup flow so the dialog doesn't open when the file is
  // opened.
  await sendTestMessage({name: 'setOfficeSetupComplete', complete: true});

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // Check that the Word file's alternate URL has been opened in a browser
  // window. The query parameter is concatenated to the URL as office files
  // opened from drive have this query parameter added
  // (https://crrev.com/c/3867338).
  await remoteCall.waitForLastOpenedBrowserTabUrl(
      ENTRIES.smallDocxHosted.alternateUrl.concat('&cros_files=true'));
};

testcase.openOfficeWordFromMyFiles = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The available Office task should be "Upload to Drive".
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

// Tests that "Upload to Drive" cannot be enabled if the "Upload Office To
// Cloud" flag is disabled (test setup similar to `openOfficeWordFromMyFiles`).
testcase.uploadToDriveRequiresUploadOfficeToCloudEnabled = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);
  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // Since the Upload Office To Cloud flag isn't enabled, the Upload to Drive
  // task should not be available: another task should have been executed
  // instead (QuickOffice or generic task).
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == openDocWithDriveDescriptor().actionId);
  chrome.test.assertFalse(
      taskDescriptor.actionId == openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxHosted]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxHosted.nameText]));

  // The Drive/Docs task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openDocWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeExcelFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallXlsxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallXlsxPinned.nameText]));

  // The Web Drive Office Excel task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openExcelWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficePowerPointFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallPptxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallPptxPinned.nameText]));

  // The Web Drive Office PowerPoint task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(openPowerPointWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openMultipleOfficeWordFromDrive = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [],
      [ENTRIES.smallDocx, ENTRIES.smallDocxPinned, ENTRIES.smallDocxHosted]);

  const enterKey = ['#file-list', 'Enter', false, false, false];

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Select all the files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check: the file-list should show 3 selected files.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElement(
        appId, '.check-select #files-selected-label');
    if (element.text !== '3 files selected') {
      return pending(
          caller, `Waiting for files to be selected, got: ${element.text}`);
    }
  });

  let taskDescriptor;
  let expectedExecuteTaskCount = 0;
  let histogramCount;

  // Wait for the tasks calculation to complete, updating the "Open" button.
  await remoteCall.waitForElement(appId, '#tasks[get-tasks-completed]');

  // Wait until the open button is available.
  await remoteCall.waitForElement(appId, '#tasks');

  // TODO(petermarshall): Check that the Docs task is available but that we fell
  // back to Quick office: one of the selected entries doesn't have a
  // "docs.google.com" alternate URL.

  // Press Enter to execute the task.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // Check that it's the Docs task.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Unselect the file that doesn't have an alternate URL.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.smallDocx.nameText}"]`,
      {ctrl: true});

  // Wait for the file to be unselected.
  await remoteCall.waitForElement(
      appId, `[file-name="${ENTRIES.smallDocx.nameText}"]:not([selected])`);

  // Press Enter.
  remoteCall.fakeKeyDown(appId, ...enterKey);

  // The Drive/Docs task should be available and executed.
  expectedExecuteTaskCount++;
  taskDescriptor = await getExecutedTask(appId, expectedExecuteTaskCount);
  chrome.test.assertEq(openDocWithDriveDescriptor(), taskDescriptor);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDriveNotSynced = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The Drive/Docs task should be available and executed.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromMyFilesOffline = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.smallDocx]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.EMPTY.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['empty']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocx.nameText]));

  // The Drive/Docs task should be executed, but it will fall back to
  // QuickOffice.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

testcase.openOfficeWordFromDriveOffline = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  // Fake chrome.fileManagerPrivate.executeTask to return
  // chrome.fileManagerPrivate.TaskResult.OPENED.
  const fakeData = {
    'chrome.fileManagerPrivate.executeTask': ['static_fake', ['opened']],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Open file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.smallDocxPinned.nameText]));

  // The Drive/Docs task should be executed, but it will fall back to
  // QuickOffice.
  const taskDescriptor = await getExecutedTask(appId);
  chrome.test.assertEq(
      taskDescriptor.actionId, openDocWithDriveDescriptor().actionId);

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};

/** Tests that the educational nudge is displayed when the preference is set. */
testcase.officeShowNudgeGoogleDrive = async () => {
  // Set the pref emulating that the user has moved a file.
  await sendTestMessage({
    name: 'setPrefOfficeFileMovedToGoogleDrive',
    timestamp: Date.now(),
  });

  // Open the Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.smallDocxPinned]);

  // Check that the nudge and its text is visible.
  await remoteCall.waitNudge(
      appId, 'Recently opened Microsoft files have moved to Google Drive');
};
