// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {createCrostiniForTest} from '../../background/js/mock_crostini.js';
import {MockProgressCenter} from '../../background/js/mock_progress_center.js';
import {metrics} from '../../common/js/metrics.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {LEGACY_FILES_EXTENSION_ID} from '../../common/js/url_constants.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {FilesPasswordDialog} from '../elements/files_password_dialog.js';

import {DirectoryChangeTracker, DirectoryModel} from './directory_model.js';
import {FileManager} from './file_manager.js';
import {FileTasks} from './file_tasks.js';
import {FileTransferController} from './file_transfer_controller.js';
import {MetadataItem} from './metadata/metadata_item.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {TaskController} from './task_controller.js';
import {TaskHistory} from './task_history.js';
import {DefaultTaskDialog} from './ui/default_task_dialog.js';
import {ImportCrostiniImageDialog} from './ui/import_crostini_image_dialog.js';
import {InstallLinuxPackageDialog} from './ui/install_linux_package_dialog.js';

/** Utility function that appends value under a given name in the store.  */
function record<T>(store: Map<string, T[]>, name: string, value: T) {
  let recorded = store.get(name);
  if (!recorded) {
    recorded = [];
    store.set(name, recorded);
  }
  recorded.push(value);
}

/**
 * A map from histogram name to all enums recorded for it.
 */
const enumMap = new Map();

/**
 * A map from histogram name to all counts recorded for it.
 */
const countMap = new Map();

/**
 * A map from histogram name to all times recorded for it.
 */
const timeMap = new Map();

let passwordDialog: FilesPasswordDialog;

/** Mock metrics.recordEnum.  */
// @ts-ignore: Remove ignore once metrics_base.recordEnum() is in TS and the
// signature is compatible.
metrics.recordEnum = function<T>(name: string, value: T, valid: T[]): void {
  assertTrue(valid.includes(value));
  record(enumMap, name, value);
};

/**
 * Mock metrics.recordSmallCount.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metrics.recordSmallCount = function(name: string, value: number) {
  record(countMap, name, value);
};

/**
 * Mock metrics.recordTime.
 * @param {string} name Short metric name.
 * @param {number} time Time to be recorded in milliseconds.
 */
metrics.recordTime = function(name: string, time: number) {
  record(timeMap, name, time);
};

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

/** Mock task history. */
const mockTaskHistory = {
  getLastExecutedTime: function(_descriptor: any) {
    return 0;
  },
  recordTaskExecuted: function(_descriptor: any) {},
} as unknown as TaskHistory;

/** Mock file transfer controller. */
const mockFileTransferController = {} as unknown as FileTransferController;

/** Mock directory change tracker. */
const fakeTracker = {
  hasChange: false,
} as unknown as DirectoryChangeTracker;

/** Fake url for mounted ZIP file. */
const fakeMountedZipUrl: string =
    'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'external/fakePath/test.zip';

/** Panel IDs for ZIP mount operations. */
const zipMountPanelId = 'Mounting: ' + fakeMountedZipUrl;

/** Panel IDs for ZIP mount errors.  */
const errorZipMountPanelId = 'Cannot mount: ' + fakeMountedZipUrl;

// Set up test components.
export function setUp() {
  // Override the translations used in the tests to make easier to compare.
  loadTimeData.overrideValues({
    NO_TASK_FOR_EXECUTABLE: 'NO_TASK_FOR_EXECUTABLE',
    NO_TASK_FOR_DMG: 'NO_TASK_FOR_DMG',
    NO_TASK_FOR_CRX_TITLE: 'NO_TASK_FOR_CRX_TITLE',
    NO_TASK_FOR_CRX: 'NO_TASK_FOR_CRX',
    NO_TASK_FOR_FILE: 'NO_TASK_FOR_FILE',
  });
  const mockTask = {
    descriptor: {
      appId: 'handler-extension-id',
      taskType: 'app',
      actionId: 'any',
    },
    isDefault: false,
    isGenericFileHandler: true,
  } as unknown as chrome.fileManagerPrivate.FileTask;

  // Mock chome APIs.
  mockChrome = {
    fileManagerPrivate: {
      getFileTasks: function(
          _entries: Entry[], _sourceUrls: string[],
          callback: (tasks: any) => void) {
        setTimeout(callback.bind(null, {tasks: [mockTask]}), 0);
      },
      executeTask: function(
          _descriptor: any, _entries: any,
          onViewFiles: (result: chrome.fileManagerPrivate.TaskResult) => void) {
        onViewFiles('failed');
      },
      sharePathsWithCrostini: function(
          _vmName: any, _entries: Entry[], _persist: any,
          callback: () => void) {
        callback();
      },
    },
  };

  installMockChrome(mockChrome);
  enumMap.clear();
  countMap.clear();
  timeMap.clear();
}

/**
 * Fail with an error message.
 * @param message The error message.
 * @param details Optional details.
 */
function failWithMessage(message: string, details?: string) {
  if (details) {
    message += ': '.concat(details);
  }
  throw new Error(message);
}

/** Returns mocked file manager components. */
function getMockFileManager(): FileManager {
  const crostini = createCrostiniForTest();

  passwordDialog = {} as unknown as FilesPasswordDialog;
  const fileManager = {
    volumeManager: /** @type {!VolumeManager} */ ({
      getLocationInfo: function(_entry: Entry) {
        return {
          rootType: VolumeManagerCommon.RootType.DRIVE,
        };
      },
      getDriveConnectionState: function() {
        return chrome.fileManagerPrivate.DriveConnectionStateType;
      },
      getVolumeInfo: function(_entry: Entry) {
        return {
          volumeType: VolumeManagerCommon.VolumeType.DRIVE,
        };
      },
    }),
    ui: /** @type {!FileManagerUI} */ ({
      alertDialog: {
        showHtml: function(
            _title: string, _text: string, _onOk: () => void,
            _onCancel: () => void, _onShow: () => void) {},
      },
      passwordDialog,
      speakA11yMessage: (_text: string) => {},
    }),
    metadataModel: {
      getCache: function(_entries: Entry[], _names: string[]) {
        return _entries.map(_ => new MetadataItem());
      },
    } as unknown as MetadataModel,
    directoryModel: {
      getCurrentRootType: function() {
        return null;
      },
      changeDirectoryEntry: function(_displayRoot: Entry) {},
    } as unknown as DirectoryModel,
    crostini: crostini,
    progressCenter: new MockProgressCenter(),
    taskController: {
      createItems(_fileTasks: FileTasks) {},
    } as unknown as TaskController,
  };

  fileManager.crostini.initVolumeManager(
      fileManager.volumeManager as unknown as VolumeManager);
  return fileManager as unknown as FileManager;
}

/**
 * Returns a promise that resolves when the showHtml method of alert dialog is
 * called with the expected title and text.
 */
function showHtmlOfAlertDialogIsCalled(
    entries: Entry[], expectedTitle: string,
    expectedText: string): Promise<void> {
  return new Promise((resolve) => {
    const fileManager = getMockFileManager();
    fileManager.ui.alertDialog.showHtml =
        (title, text, _onOk, _onCancel, _onShow) => {
          assertEquals(expectedTitle, title);
          assertEquals(expectedText, text);
          resolve();
        };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, mockTaskHistory,
            fileManager.crostini, fileManager.progressCenter,
            fileManager.taskController)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise that resolves when the task picker is called.
 */
function showDefaultTaskDialogCalled(entries: Entry[]): Promise<void> {
  return new Promise((resolve) => {
    const fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          _title, _message, _items, _defaultIdx, _onSuccess) {
        resolve();
      },
    } as DefaultTaskDialog;

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, mockTaskHistory,
            fileManager.crostini, fileManager.progressCenter,
            fileManager.taskController)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise that resolves when showImportCrostiniImageDialog is called.
 */
async function showImportCrostiniImageDialogIsCalled(entries: Entry[]):
    Promise<void> {
  return new Promise((resolve) => {
    const fileManager = getMockFileManager();
    fileManager.ui.importCrostiniImageDialog = {
      showImportCrostiniImageDialog: (_entry: Entry) => {
        resolve();
      },
    } as ImportCrostiniImageDialog;

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui,
            mockFileTransferController, entries, mockTaskHistory,
            fileManager.crostini, fileManager.progressCenter,
            fileManager.taskController)
        .then(tasks => {
          tasks.executeDefault();
        });
  });
}

/** Tests opening a .exe file. */
export async function testToOpenExeFile(done: () => void) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.exe');

  await showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.exe', 'NO_TASK_FOR_EXECUTABLE');
  done();
}

/** Tests opening a .dmg file. */
export async function testToOpenDmgFile(done: () => void) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.dmg');

  await showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.dmg', 'NO_TASK_FOR_DMG');
  done();
}

/**
 * Tests opening a .crx file.
 */
export async function testToOpenCrxFile(done: () => void) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.crx');

  await showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'NO_TASK_FOR_CRX_TITLE', 'NO_TASK_FOR_CRX');
  done();
}

/** Tests opening a .rtf file. */
export async function testToOpenRtfFile(done: () => void) {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.rtf');

  await showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.rtf', 'NO_TASK_FOR_FILE');
  done();
}

/**
 * Tests opening the task picker with an entry that does not have a default app
 * but there are multiple apps that could open it.
 */
export async function testOpenTaskPicker(done: () => void) {
  chrome.fileManagerPrivate.getFileTasks =
      (_entries: Entry[], _sourceUrls: string[],
       callback: (tasks: any) => void) => {
        setTimeout(
            callback.bind(null, {
              tasks: [
                {
                  descriptor: {
                    appId: 'handler-extension-id1',
                    taskType: 'app',
                    actionId: 'any',
                  },
                  isDefault: false,
                  isGenericFileHandler: false,
                  title: 'app 1',
                },
                {
                  descriptor: {
                    appId: 'handler-extension-id2',
                    taskType: 'app',
                    actionId: 'any',
                  },
                  isDefault: false,
                  isGenericFileHandler: false,
                  title: 'app 2',
                },
              ],
            }),
            0);
      };

  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.tiff');

  await showDefaultTaskDialogCalled([mockEntry]);
  done();
}

/**
 * Tests opening the task picker with an entry that does not have a default app
 * but there are multiple apps that could open it. The app with the most recent
 * task execution order should execute.
 */
export async function testOpenWithMostRecentlyExecuted(done: () => void) {
  const latestTaskDescriptor = {
    appId: 'handler-extension-most-recently-executed',
    taskType: 'app',
    actionId: 'any',
  };
  const oldTaskDescriptor = {
    appId: 'handler-extension-executed-before',
    taskType: 'app',
    actionId: 'any',
  };

  chrome.fileManagerPrivate.getFileTasks =
      (_entries: Entry[], _sourceUrls: string[],
       callback: (tasks: chrome.fileManagerPrivate.ResultingTasks) => void) => {
        setTimeout(
            callback.bind(
                null,
                // File tasks is sorted by last executed time, latest first.
                {
                  tasks: [
                    {
                      descriptor: latestTaskDescriptor,
                      isDefault: false,
                      isGenericFileHandler: false,
                      title: 'app 1',
                    },
                    {
                      descriptor: oldTaskDescriptor,
                      isDefault: false,
                      isGenericFileHandler: false,
                      title: 'app 2',
                    },
                    {
                      descriptor: {
                        appId: 'handler-extension-never-executed',
                        taskType: 'app',
                        actionId: 'any',
                      },
                      isDefault: false,
                      isGenericFileHandler: false,
                      title: 'app 3',
                    },
                  ],
                } as chrome.fileManagerPrivate.ResultingTasks),
            0);
      };

  const taskHistory = /** @type {!TaskHistory} */ ({
    getLastExecutedTime: function(
        descriptor: chrome.fileManagerPrivate.FileTaskDescriptor) {
      if (util.descriptorEqual(descriptor, oldTaskDescriptor)) {
        return 10000;
      }
      if (util.descriptorEqual(descriptor, latestTaskDescriptor)) {
        return 20000;
      }
      return 0;
    },
    recordTaskExecuted: function(
        _descriptor: chrome.fileManagerPrivate.FileTaskDescriptor) {},
  });

  type FileTaskDescriptor = chrome.fileManagerPrivate.FileTaskDescriptor;
  let executedTask: FileTaskDescriptor|null = null;
  chrome.fileManagerPrivate.executeTask =
      (descriptor: FileTaskDescriptor, _entries: Entry[],
       onViewFiles: (result: chrome.fileManagerPrivate.TaskResult) => void) => {
        executedTask = descriptor;
        onViewFiles(chrome.fileManagerPrivate.TaskResult.OPENED);
      };

  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.tiff');

  const fileManager = getMockFileManager();
  fileManager.ui.defaultTaskPicker = {
    showDefaultTaskDialog: function(
        _title, _message, _items, _defaultIdx, _onSuccess) {
      failWithMessage('should not show task picker');
    },
  } as DefaultTaskDialog;

  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [mockEntry], taskHistory as TaskHistory, fileManager.crostini,
      fileManager.progressCenter, fileManager.taskController);
  await tasks.executeDefault();
  assertTrue(util.descriptorEqual(latestTaskDescriptor, executedTask!));

  done();
}

function setUpInstallLinuxPackage() {
  const fileManager = getMockFileManager();
  fileManager.volumeManager.getLocationInfo = (_entry): EntryLocation => {
    return {
      rootType: VolumeManagerCommon.RootType.CROSTINI,
    } as unknown as EntryLocation;
  };
  const fileTask = {
    descriptor: {
      appId: LEGACY_FILES_EXTENSION_ID,
      taskType: 'app',
      actionId: 'install-linux-package',
    },
    isDefault: false,
    isGenericFileHandler: false,
    title: '__MSG_INSTALL_LINUX_PACKAGE__',
  };
  chrome.fileManagerPrivate.getFileTasks =
      (_entries: Entry[], _sourceUrls: string[],
       callback: (tasks: any) => void) => {
        setTimeout(callback.bind(null, {tasks: [fileTask]}), 0);
      };
  return fileManager;
}

/**
 * Tests opening a .deb file. The crostini linux package install dialog should
 * be called.
 */
export async function testOpenInstallLinuxPackageDialog(done: () => void) {
  const fileManager = setUpInstallLinuxPackage();
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.deb');

  await new Promise<void>(async (resolve) => {
    fileManager.ui.installLinuxPackageDialog = {
      showInstallLinuxPackageDialog: function(_entry: Entry) {
        resolve();
      },
    } as unknown as InstallLinuxPackageDialog;

    const tasks = await FileTasks.create(
        fileManager.volumeManager, fileManager.metadataModel,
        fileManager.directoryModel, fileManager.ui, mockFileTransferController,
        [mockEntry], mockTaskHistory, fileManager.crostini,
        fileManager.progressCenter, fileManager.taskController);
    await tasks.executeDefault();
  });

  done();
}

/**
 * Tests opening a .tini file. The import crostini image dialog should be
 * called.
 */
export async function testToOpenTiniFileOpensImportCrostiniImageDialog(
    done: () => void) {
  chrome.fileManagerPrivate.getFileTasks =
      (_entries: Entry[], _sourceUrls: string[],
       callback: (tasks: any) => void) => {
        setTimeout(
            callback.bind(null, {
              tasks: [
                {
                  descriptor: {
                    appId: LEGACY_FILES_EXTENSION_ID,
                    taskType: 'app',
                    actionId: 'import-crostini-image',
                  },
                  isDefault: false,
                  isGenericFileHandler: false,
                },
              ],
            }),
            0);
      };

  const mockEntry =
      MockFileEntry.create(new MockFileSystem('testfilesystem'), '/test.tini');

  await showImportCrostiniImageDialogIsCalled([mockEntry]);
  done();
}

/**
 * Checks that the function that returns a file type for file entry handles
 * correctly identifies files with known and unknown extensions.
 */
export function testGetViewFileType() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const testData = [
    {extension: 'log', expected: '.log'},
    {extension: '__unknown_extension__', expected: 'other'},
  ];
  for (const data of testData) {
    const mockEntry =
        MockFileEntry.create(mockFileSystem, `/report.${data.extension}`);
    const type = FileTasks.getViewFileType(mockEntry);
    assertEquals(data.expected, type);
  }
}

/**
 * Checks that the progress center is properly updated when mounting archives
 * successfully.
 * @suppress {visibility}
 */
export async function testMountArchiveAndChangeDirectoryNotificationSuccess(
    done: () => void) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], mockTaskHistory, fileManager.crostini, fileManager.progressCenter,
      fileManager.taskController);

  fileManager.volumeManager!.mountArchive =
      async function(_url: string, _password: string): Promise<VolumeInfo> {
    // Check: progressing state.
    assertEquals(
        ProgressItemState.PROGRESSING,
        fileManager.progressCenter.getItemById(zipMountPanelId)?.state);

    const volumeInfo = {resolveDisplayRoot: () => null};
    return volumeInfo as unknown as VolumeInfo;
  };

  // Mount archive.
  await tasks['mountArchiveAndChangeDirectory_'](
      fakeTracker, fakeMountedZipUrl);

  // Check: mount completed, no error.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId)?.state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  // Check: a zip mount time UMA has been recorded.
  assertTrue(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an archive
 * resolves with an error.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationInvalidArchive(done: () => void) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], mockTaskHistory, fileManager.crostini, fileManager.progressCenter,
      fileManager.taskController);

  fileManager.volumeManager.mountArchive = function(_url, _password) {
    return Promise.reject(VolumeManagerCommon.VolumeError.INTERNAL_ERROR);
  };

  // Mount archive.
  await tasks['mountArchiveAndChangeDirectory_'](
      fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed with an error.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId)?.state);
  assertEquals(
      ProgressItemState.ERROR,
      fileManager.progressCenter.getItemById(errorZipMountPanelId)?.state);

  // Check: no zip mount time UMA has been recorded since mounting the archive
  // failed.
  assertFalse(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when the password prompt
 * for an encrypted archive is canceled.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationCancelPassword(done: () => void) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], mockTaskHistory, fileManager.crostini, fileManager.progressCenter,
      fileManager.taskController);

  fileManager.volumeManager.mountArchive = function(_url, _password) {
    return Promise.reject(VolumeManagerCommon.VolumeError.NEED_PASSWORD);
  };

  passwordDialog.askForPassword =
      async function(_filename: string, _password: string|null = null) {
    return Promise.reject(FilesPasswordDialog.USER_CANCELLED);
  };

  // Mount archive.
  await tasks['mountArchiveAndChangeDirectory_'](
      fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed, no error since the user canceled the password
  // prompt.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId)?.state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  // Check: no zip mount time UMA has been recorded since the mount has been
  // cancelled.
  assertFalse(timeMap.has('ZipMountTime.Other'));

  done();
}

/**
 * Checks that the progress center is properly updated when mounting an
 * encrypted archive.
 * @suppress {visibility}
 */
export async function
testMountArchiveAndChangeDirectoryNotificationEncryptedArchive(
    done: () => void) {
  const fileManager = getMockFileManager();

  // Define FileTasks instance.
  const tasks = await FileTasks.create(
      fileManager.volumeManager, fileManager.metadataModel,
      fileManager.directoryModel, fileManager.ui, mockFileTransferController,
      [], mockTaskHistory, fileManager.crostini, fileManager.progressCenter,
      fileManager.taskController);

  fileManager.volumeManager.mountArchive = function(
      _url, password: string|null) {
    return new Promise<VolumeInfo>((resolve, reject) => {
      if (password) {
        assertEquals('testpassword', password);
        const volumeInfo = {resolveDisplayRoot: () => null};
        resolve(volumeInfo as unknown as VolumeInfo);
      } else {
        reject(VolumeManagerCommon.VolumeError.NEED_PASSWORD);
      }
    });
  };

  passwordDialog.askForPassword =
      async function(_filename: string, _password: string|null = null) {
    return Promise.resolve('testpassword');
  };

  // Mount archive.
  await tasks['mountArchiveAndChangeDirectory_'](
      fakeTracker, fakeMountedZipUrl);

  // Check: mount is completed, no error since the user entered a valid
  // password.
  assertEquals(
      ProgressItemState.COMPLETED,
      fileManager.progressCenter.getItemById(zipMountPanelId)?.state);
  assertEquals(
      undefined, fileManager.progressCenter.getItemById(errorZipMountPanelId));

  done();
}
