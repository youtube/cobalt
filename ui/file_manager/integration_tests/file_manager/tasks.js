// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCaller, pending, repeatUntil, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';
import {FILE_MANAGER_EXTENSIONS_ID} from './test_data.js';

/**
 * Fake task.
 */
export class FakeTask {
  /**
   * @param {boolean} isDefault Whether the task is default or not.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor Task
   *     descriptor.
   * @param {string=} opt_title Title of the task.
   * @param {boolean=} opt_isGenericFileHandler Whether the task is a generic
   *     file handler.
   * @param {boolean=} opt_isDlpBlocked Whether the task is blocked by DLP.
   */
  constructor(
      isDefault, descriptor, opt_title, opt_isGenericFileHandler,
      opt_isDlpBlocked) {
    this.driveApp = false;
    this.iconUrl = 'chrome://theme/IDR_DEFAULT_FAVICON';  // Dummy icon
    this.isDefault = isDefault;
    this.descriptor = descriptor;
    this.title = opt_title;
    this.isGenericFileHandler = opt_isGenericFileHandler || false;
    this.isDlpBlocked = opt_isDlpBlocked || false;
    Object.freeze(this);
  }
}

/**
 * Fake tasks for a local volume.
 *
 * @type {Array<FakeTask>}
 * @const
 */
export const DOWNLOADS_FAKE_TASKS = [
  new FakeTask(
      true,
      {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
      'DummyTask1'),
  new FakeTask(
      false,
      {appId: 'dummytaskid-2', taskType: 'fake-type', actionId: 'open-with'},
      'DummyTask2'),
];

/**
 * Fake tasks for a local volume opening in browser.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DOWNLOADS_FAKE_TEXT = [
  new FakeTask(true, {
    appId: FILE_MANAGER_EXTENSIONS_ID,
    taskType: 'file',
    actionId: 'view-in-browser',
  }),
];

/**
 * Fake tasks for a PDF file opening in browser.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DOWNLOADS_FAKE_PDF = [
  new FakeTask(true, {
    appId: FILE_MANAGER_EXTENSIONS_ID,
    taskType: 'file',
    actionId: 'view-as-pdf',
  }),
];

/**
 * Fake tasks for a drive volume.
 *
 * @type {Array<FakeTask>}
 * @const
 */
const DRIVE_FAKE_TASKS = [
  new FakeTask(
      true, {appId: 'dummytaskid', taskType: 'drive', actionId: 'open-with'},
      'DummyTask1'),
  new FakeTask(
      false, {appId: 'dummytaskid-2', taskType: 'drive', actionId: 'open-with'},
      'DummyTask2'),
];

/**
 * Sets up task tests.
 *
 * @param {string} rootPath Root path.
 * @param {Array<FakeTask>} fakeTasks Fake tasks.
 */
async function setupTaskTest(rootPath, fakeTasks) {
  const appId = await setupAndWaitUntilReady(rootPath);
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [fakeTasks]);
  return appId;
}

/**
 * Tests executing the default task when there is only one task.
 *
 * @param {string} appId Window ID.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor Task
 *     descriptor.
 */
async function executeDefaultTask(appId, descriptor) {
  // Select file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Double-click the file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li.table-row[selected] .filename-label span']));

  // Wait until the task is executed.
  await remoteCall.waitUntilTaskExecutes(appId, descriptor, ['hello.txt']);
}

/**
 * Tests to specify default task via the default task dialog.
 *
 * @param {string} appId Window ID.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor Task
 *     descriptor of the task expected to be newly specified as default.
 * @return {Promise} Promise to be fulfilled/rejected depends on the test
 *     result.
 */
async function defaultTaskDialog(appId, descriptor) {
  // Prepare expected labels.
  const expectedLabels = [
    'DummyTask1 (default)',
    'DummyTask2',
  ];

  // Select file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Click the change default menu.
  await remoteCall.waitForElement(appId, '#tasks[multiple]');
  await remoteCall.waitForElement(appId, '#tasks-menu .change-default');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId,
      ['#tasks', 'select', {item: {type: 'ChangeDefaultTask'}}]));

  const caller = getCaller();

  // Wait for the list of menu item is added as expected.
  await repeatUntil(async () => {
    // Obtains menu items.
    const items = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId,
        ['#default-task-dialog #default-tasks-list li']);

    // Compare the contents of items.
    const actualLabels = items.map((item) => item.text);
    if (chrome.test.checkDeepEq(expectedLabels, actualLabels)) {
      return true;
    }
    return pending(
        caller, 'Tasks do not match, expected: %j, actual: %j.', expectedLabels,
        actualLabels);
  });

  // Click the non default item.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeEvent', appId, [
        '#default-task-dialog #default-tasks-list li:nth-of-type(2)',
        'mousedown',
        {bubbles: true, button: 0},
      ]));
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeEvent', appId, [
        '#default-task-dialog #default-tasks-list li:nth-of-type(2)',
        'click',
        {bubbles: true},
      ]));

  // Wait for the dialog hidden, and the task is executed.
  await remoteCall.waitForElementLost(appId, '#default-task-dialog');

  // Execute the new default task. Click on "Open ▼" button.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['#tasks']);

  // Wait for dropdown menu to show.
  await remoteCall.waitForElement(
      appId, '#tasks-menu:not([hidden]) cr-menu-item');

  // Click on first menu item.
  remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#tasks-menu:not([hidden]) cr-menu-item:nth-child(1)']);

  // Wait dropdown menu to hide.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '#tasks-menu[hidden]'));

  // Check the executed tasks.
  await remoteCall.waitUntilTaskExecutes(appId, descriptor, ['hello.txt']);
}

testcase.executeDefaultTaskDrive = async () => {
  const appId = await setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS);
  await executeDefaultTask(appId, DRIVE_FAKE_TASKS[0].descriptor);
};

testcase.executeDefaultTaskDownloads = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS);
  await executeDefaultTask(appId, DOWNLOADS_FAKE_TASKS[0].descriptor);
};

testcase.defaultTaskForTextPlain = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TEXT);
  await executeDefaultTask(appId, DOWNLOADS_FAKE_TEXT[0].descriptor);
};

testcase.defaultTaskForPdf = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_PDF);
  await executeDefaultTask(appId, DOWNLOADS_FAKE_PDF[0].descriptor);
};

testcase.defaultTaskDialogDrive = async () => {
  const appId = await setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS);
  await defaultTaskDialog(appId, DRIVE_FAKE_TASKS[1].descriptor);
};

testcase.defaultTaskDialogDownloads = async () => {
  const appId = await setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS);
  await defaultTaskDialog(appId, DOWNLOADS_FAKE_TASKS[1].descriptor);
};


/**
 * Tests that the Change Default Task dialog has a scrollable list.
 */
testcase.changeDefaultDialogScrollList = async () => {
  const tasks = [
    new FakeTask(
        true,
        {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask1'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-2', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask2'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask3'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask4'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask5'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask6'),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask7'),
  ];

  // Override tasks for the test.
  const appId = await setupTaskTest(RootPath.DOWNLOADS, tasks);

  // Select file.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');

  // Click the change default task menu.
  await remoteCall.waitForElement(appId, '#tasks[multiple]');
  await remoteCall.waitForElement(appId, '#tasks-menu .change-default');
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId,
      ['#tasks', 'select', {item: {type: 'ChangeDefaultTask'}}]));

  // Wait for Default Task Dialog with the scrollable list CSS class.
  await remoteCall.waitForElement(
      appId, '#default-task-dialog.scrollable-list');

  // Check: The dialog should start with bottom-shadow CSS class.
  await remoteCall.waitForElement(appId, '#default-task-dialog.bottom-shadow');

  // Scroll down the list in the dialog.
  await remoteCall.callRemoteTestUtil(
      'setScrollTop', appId, ['#default-task-dialog list', 100]);

  // Check: CSS class bottom-shadow should be removed.
  await remoteCall.waitForElementLost(
      appId, '#default-task-dialog.bottom-shadow');
};

testcase.genericTaskIsNotExecuted = async () => {
  const tasks = [new FakeTask(
      false,
      {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
      'DummyTask1', true /* isGenericFileHandler */)];

  // When default task is not set, executeDefaultInternal_ in file_tasks.js
  // tries to show it in a browser tab. By checking the view-in-browser task is
  // executed, we check that default task is not set in this situation.
  //
  // See: src/ui/file_manager/file_manager/foreground/js/file_tasks.js&l=404
  const appId = await setupTaskTest(RootPath.DOWNLOADS, tasks);
  await executeDefaultTask(appId, {
    appId: FILE_MANAGER_EXTENSIONS_ID,
    taskType: 'file',
    actionId: 'view-in-browser',
  });
};

testcase.genericTaskAndNonGenericTask = async () => {
  const tasks = [
    new FakeTask(
        false,
        {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask1', true /* isGenericFileHandler */),
    new FakeTask(
        false,
        {appId: 'dummytaskid-2', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask2', false /* isGenericFileHandler */),
    new FakeTask(
        false,
        {appId: 'dummytaskid-3', taskType: 'fake-type', actionId: 'open-with'},
        'DummyTask3', true /* isGenericFileHandler */),
  ];

  const appId = await setupTaskTest(RootPath.DOWNLOADS, tasks);
  await executeDefaultTask(appId, tasks[1].descriptor);
};

testcase.noActionBarOpenForDirectories = async () => {
  const fileTasks = [new FakeTask(
      true,
      {appId: 'dummytaskid', taskType: 'fake-type', actionId: 'open-with'},
      'FileTask1')];

  const dirTasks = [
    new FakeTask(
        true,
        {appId: 'dummytaskid-1', taskType: 'fake-type', actionId: 'open-with'},
        'DirTask1'),
    new FakeTask(
        true,
        {appId: 'dummytaskid-2', taskType: 'fake-type', actionId: 'open-with'},
        'DirTask2'),
  ];

  // Override tasks for the test.
  const appId = await setupTaskTest(RootPath.DOWNLOADS, fileTasks);

  // Select file and ensure action bar open is shown.
  await remoteCall.waitUntilSelected(appId, 'hello.txt');
  await remoteCall.waitForElement(appId, '#tasks:not([hidden])');

  // Select dir and ensure action bar open is hidden, but context menu is shown.
  // Use different tasks for dir.
  await remoteCall.callRemoteTestUtil('overrideTasks', appId, [dirTasks]);
  await remoteCall.waitUntilSelected(appId, 'photos');
  await remoteCall.waitForElement(appId, '#tasks[hidden]');
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list .table-row[selected]']));
  await remoteCall.waitForElement(
      appId, '#default-task-menu-item:not([hidden])');

  // Click 'Open with'.
  await remoteCall.waitAndClickElement(
      appId, 'cr-menu-item[command="#open-with"]:not([hidden])');
  // Ensure apps are shown.
  await remoteCall.waitForElement(appId, '#tasks-menu:not([hidden])');
  const appOptions = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['#tasks-menu [tabindex]']);
  chrome.test.assertEq(3, appOptions.length);
  chrome.test.assertEq('DirTask1 (default)', appOptions[0].text);
  chrome.test.assertEq('DirTask2', appOptions[1].text);
  chrome.test.assertEq('Change default…', appOptions[2].text);
};

testcase.executeViaDblClick = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);
  //  Double-click the file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li[file-name="hello.txt"] .filename-label span']));

  // Wait until the task is executed.
  const descriptor = DOWNLOADS_FAKE_TASKS[0].descriptor;
  await remoteCall.waitUntilTaskExecutes(appId, descriptor, ['hello.txt']);

  // Reset the overridden tasks.
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Click on the currently selected tree item to reset the file list selection.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree .tree-item[selected]');

  // Double click on a different file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li[file-name="world.ogv"] .filename-label span']));

  // Check the tasks again.
  await remoteCall.waitUntilTaskExecutes(appId, descriptor, ['world.ogv']);
};
