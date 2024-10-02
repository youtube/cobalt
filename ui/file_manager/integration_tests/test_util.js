// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RemoteCall} from './remote_call.js';

/**
 * Sends a command to the controlling test harness, namely and usually, the
 * chrome FileManagerBrowserTest harness: it expects the command to contain
 * the 'name' of the command, and any required or optional arguments of the
 * command, e.g.,
 *
 *   await sendTestMessage({
 *     name: 'addEntries', // command with volume and entries arguments
 *     volume: volume,
 *     entries: entries
 *   });
 *
 * @param {Object} command Test command to send. The object is converted to
 *     a JSON string prior to sending.
 * @return {Promise} Promise to be fulfilled with the value returned by the
 *     chrome.test.sendMessage callback.
 */
export function sendTestMessage(command) {
  if (typeof command.name === 'string') {
    return new Promise(function(fulfill) {
      chrome.test.sendMessage(JSON.stringify(command), fulfill);
    });
  } else {
    const error = 'sendTestMessage requires a command.name <string>';
    throw new Error(error);
  }
}

/**
 * Wait (aka pause, or sleep) for the given time in milliseconds.
 * @param {number} time Time in milliseconds.
 * @return {Promise} Promise that will resolve after Time in milliseconds
 *     has elapsed.
 */
export function wait(time) {
  return new Promise(function(resolve) {
    setTimeout(resolve, time);
  });
}

/**
 * Verifies if there are no Javascript errors in the given app window by
 * asserting the count returned by the app.getErrorCount remote call.
 * @param {!RemoteCall} app RemoteCall interface to the app window.
 * @param {function()=} callback Completion callback.
 * @return {Promise} Promise to be fulfilled on completion.
 */
export async function checkIfNoErrorsOccuredOnApp(app, callback) {
  const count = await app.callRemoteTestUtil('getErrorCount', null, []);
  chrome.test.assertEq(0, count, 'The error count is not 0.');
  if (callback) {
    callback();
  }
}

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise to add the check to.
 * @param {Array<!RemoteCall>} apps An array of RemoteCall interfaces.
 */
export async function testPromiseAndApps(promise, apps) {
  const finished = chrome.test.callbackPass(function() {
    // The callbackPass is necessary to avoid prematurely finishing tests.
    // Don't use chrome.test.succeed() here to avoid doubled success log.
  });
  try {
    await promise;
    await Promise.all(apps.map(app => checkIfNoErrorsOccuredOnApp(app)));
  } catch (error) {
    chrome.test.fail(error.stack || error);
    return;
  }
  finished();
}

/**
 * Interval milliseconds between checks of repeatUntil.
 * @type {number}
 * @const
 */
export const REPEAT_UNTIL_INTERVAL = 200;

/**
 * Interval milliseconds between log output of repeatUntil.
 * @type {number}
 * @const
 */
export const LOG_INTERVAL = 3000;

/**
 * Returns caller's file, function and line/column number from the call stack.
 * @return {string} String with the caller's file name and line/column number,
 *     as returned by exception stack trace. Example "at /a_file.js:1:1".
 */
export function getCaller() {
  const error = new Error('For extracting error.stack');
  const ignoreStackLines = 3;
  const lines = error.stack.split('\n');
  if (ignoreStackLines < lines.length) {
    const caller = lines[ignoreStackLines];
    // Strip 'chrome-extension://oobinhbdbiehknkpbpejbbpdbkdjmoco' prefix.
    return caller.replace(/(chrome-extension:\/\/\w*)/gi, '').trim();
  }
  return '';
}


/**
 * Returns a pending marker. See also the repeatUntil function.
 * @param {string} caller name of test function that originated the operation,
 *     it's the return of getCaller() function.
 * @param {string} message Pending reason including %s, %d, or %j markers. %j
 *     format an object as JSON.
 * @param {...*} var_args Values to be assigined to %x markers.
 * @return {Object} Object which returns true for the expression: obj instanceof
 *     pending.
 */
export function pending(caller, message, var_args) {
  // |index| is used to ignore caller and message arguments subsisting markers
  // (%s, %d and %j) within message with the remaining |arguments|.
  let index = 2;
  const args = arguments;
  message = String(message);
  const formattedMessage = message.replace(/%[sdj]/g, function(pattern) {
    const arg = args[index++];
    switch (pattern) {
      case '%s':
        return String(arg);
      case '%d':
        return Number(arg);
      case '%j':
        return JSON.stringify(arg);
      default:
        return pattern;
    }
  });
  const pendingMarker = Object.create(pending.prototype);
  pendingMarker.message = caller + ': ' + formattedMessage;
  return pendingMarker;
}

/**
 * Waits until the checkFunction returns a value but a pending marker.
 * @param {function():*} checkFunction Function to check a condition. It can
 *     return a pending marker created by a pending function.
 * @return {!Promise} Promise to be fulfilled with the return value of
 *     checkFunction when the checkFunction reutrns a value but a pending
 *     marker.
 */
export async function repeatUntil(checkFunction) {
  let logTime = Date.now() + LOG_INTERVAL;
  while (true) {
    const result = await checkFunction();
    if (!(result instanceof pending)) {
      return result;
    }
    if (Date.now() > logTime) {
      console.warn(result.message);
      logTime += LOG_INTERVAL;
    }
    await wait(REPEAT_UNTIL_INTERVAL);
  }
}

/**
 * Sends the test |command| to the browser test harness and awaits a 'string'
 * result. Calls |callback| with that result.
 * @param {Object} command Test command to send. Refer to sendTestMessage()
 *    above for the expected format of a test |command| object.
 * @param {function(string)} callback Completion callback.
 * @param {Object=} opt_debug If truthy, log the result.
 */
export async function sendBrowserTestCommand(command, callback, opt_debug) {
  const caller = getCaller();
  if (typeof command.name !== 'string') {
    chrome.test.fail('Invalid test command: ' + JSON.stringify(command));
  }
  const result = await repeatUntil(async () => {
    const tryAgain = pending(caller, 'Sent BrowserTest ' + command.name);
    try {
      const result = await sendTestMessage(command);
      if (typeof result !== 'string') {
        return tryAgain;
      }
      return result;
    } catch (error) {
      console.log(error.stack || error);
      return tryAgain;
    }
  });
  if (opt_debug) {
    console.log('BrowserTest ' + command.name + ': ' + result);
  }
  callback(result);
}

/**
 * Get all the browser windows.
 * @param {number} expectedInitialCount The number of windows expected before
 *     opening a new one.
 * @return {Object} Object returned from chrome.windows.getAll().
 */
export async function getBrowserWindows(expectedInitialCount = 0) {
  const caller = getCaller();
  return repeatUntil(async () => {
    const result = await new Promise(function(fulfill) {
      chrome.windows.getAll({'populate': true}, fulfill);
    });
    if (result.length === expectedInitialCount) {
      return pending(caller, 'getBrowserWindows ' + result.length);
    }
    return result;
  });
}

/**
 * Adds the given entries to the target volume(s).
 * @param {Array<string>} volumeNames Names of target volumes.
 * @param {Array<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)=} opt_callback Callback function to be passed the
 *     result of function. The argument is true on success.
 * @return {Promise} Promise to be fulfilled when the entries are added.
 */
export async function addEntries(volumeNames, entries, opt_callback) {
  if (volumeNames.length == 0) {
    opt_callback && opt_callback(true);
    return;
  }
  const volumeResultPromises = volumeNames.map(function(volume) {
    return sendTestMessage({
      name: 'addEntries',
      volume: volume,
      entries: entries,
    });
  });
  if (!opt_callback) {
    return volumeResultPromises;
  }
  try {
    await Promise.all(volumeResultPromises);
  } catch (error) {
    opt_callback(false);
    throw error;
  }
  opt_callback(true);
}

/**
 * @enum {string}
 * @const
 */
export const EntryType = {
  FILE: 'file',
  DIRECTORY: 'directory',
  LINK: 'link',
  SHARED_DRIVE: 'team_drive',
  COMPUTER: 'Computer',
};
Object.freeze(EntryType);

/**
 * @enum {string}
 * @const
 */

export const SharedOption = {
  NONE: 'none',
  SHARED: 'shared',
  SHARED_WITH_ME: 'sharedWithMe',
  NESTED_SHARED_WITH_ME: 'nestedSharedWithMe',
};
Object.freeze(SharedOption);


/**
 * @typedef {{
 *   downloads: string,
 *   drive: string,
 *   android_files: string,
 * }}
 *
 */
export let getRootPathsResult;

/**
 * @typedef {{
 *   DOWNLOADS: string,
 *   DRIVE: string,
 *   ANDROID_FILES: string,
 * }}
 */
export const RootPath = {
  DOWNLOADS: '/must-be-filled-in-test-setup',
  DRIVE: '/must-be-filled-in-test-setup',
  ANDROID_FILES: '/must-be-filled-in-test-setup',
};
Object.seal(RootPath);


/**
 * The capabilities (permissions) for the Test Entry. Structure should match
 * TestEntryCapabilities in file_manager_browsertest_base.cc. All capabilities
 * default to true if not specified.
 *
 * @typedef {{
 *    canCopy: (boolean|undefined),
 *    canDelete: (boolean|undefined),
 *    canRename: (boolean|undefined),
 *    canAddChildren: (boolean|undefined),
 *    canShare: (boolean|undefined),
 * }}
 */
export let TestEntryCapabilities;

/**
 * The folder features for the test entry. Structure should match
 * TestEntryFolderFeature in file_manager_browsertest_base.cc. All features
 * default to false is not specified.
 *
 * @typedef {{
 *    isMachineRoot: (boolean|undefined),
 *    isArbitrarySyncFolder: (boolean|undefined),
 *    isExternalMedia: (boolean|undefined),
 * }}
 */
export let TestEntryFolderFeature;

/**
 * Parameters to creat a Test Entry in the file manager. Structure should match
 * TestEntryInfo in file_manager_browsertest_base.cc.
 *
 * Field details:
 *
 * sourceFileName: Source file name that provides file contents (file location
 * relative to /chrome/test/data/chromeos/file_manager/).
 *
 * targetPath: Name of entry on the test file system. Used to determine the
 * actual name of the file.
 *
 * teamDriveName: Name of the team drive this entry is in. Defaults to a blank
 * string (no team drive). Team Drive names must be unique.
 *
 * computerName: Name of the computer this entry is in. Defaults to a blank
 * string (no computer). Computer names must be unique.
 *
 * lastModifiedTime: Last modified time as a text to be shown in the last
 * modified column.
 *
 * nameText: File name to be shown in the name column.
 *
 * sizeText: Size text to be shown in the size column.
 *
 * typeText: Type name to be shown in the type column.
 *
 * capabilities:  Capabilities of this file. Defaults to all capabilities
 * available (read-write access).
 *
 * folderFeature: Folder features of this file. Defaults to all features
 * disabled.
 *
 * pinned: Drive pinned status of this file. Defaults to false.
 *
 * availableOffline: Whether the file is available offline. Defaults to false.
 *
 * alternateUrl: File's Drive alternate URL. Defaults to an empty string.
 *
 * @typedef {{
 *    type: EntryType,
 *    sourceFileName: (string|undefined),
 *    targetPath: (string|undefined),
 *    teamDriveName: (string|undefined),
 *    computerName: (string|undefined),
 *    mimeType: (string|undefined),
 *    sharedOption: (SharedOption|undefined),
 *    lastModifiedTime: (string|undefined),
 *    nameText: (string|undefined),
 *    sizeText: (string|undefined),
 *    typeText: (string|undefined),
 *    capabilities: (TestEntryCapabilities|undefined),
 *    folderFeature: (TestEntryFolderFeature|undefined),
 *    pinned: (boolean|undefined),
 *    availableOffline: (boolean|undefined),
 *    alternateUrl: (string|undefined),
 * }}
 */
export let TestEntryInfoOptions;

/**
 * File system entry information for tests. Structure should match TestEntryInfo
 * in file_manager_browsertest_base.cc
 * TODO(sashab): Remove this, rename TestEntryInfoOptions to TestEntryInfo and
 * set the defaults in the record definition above.
 */
export class TestEntryInfo {
  /**
   * @param {TestEntryInfoOptions} options Parameters to create the
   *     TestEntryInfo.
   */
  constructor(options) {
    this.type = options.type;
    this.sourceFileName = options.sourceFileName || '';
    this.thumbnailFileName = options.thumbnailFileName || '';
    this.targetPath = options.targetPath;
    this.teamDriveName = options.teamDriveName || '';
    this.computerName = options.computerName || '';
    this.mimeType = options.mimeType || '';
    this.sharedOption = options.sharedOption || SharedOption.NONE;
    this.lastModifiedTime = options.lastModifiedTime;
    this.nameText = options.nameText || '';
    this.sizeText = options.sizeText || '';
    this.typeText = options.typeText || '';
    this.capabilities = options.capabilities;
    this.folderFeature = options.folderFeature;
    this.pinned = !!options.pinned;
    this.availableOffline = !!options.availableOffline;
    this.alternateUrl = options.alternateUrl || '';
    Object.freeze(this);
  }

  /**
   * Obtains the expected row contents for each file.
   * @param {!Array<!TestEntryInfo>} entries
   * @return {!Array<!Array<string>>}
   */
  static getExpectedRows(entries) {
    return entries.map(function(entry) {
      return entry.getExpectedRow();
    });
  }

  /**
   * Obtains a expected row contents of the file in the file list.
   * @return {!Array<string>}
   */
  getExpectedRow() {
    return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
  }

  /**
   * Returns a new entry with modified attributes specified in the
   * `newOptions` object.
   * @param {!Object} newOptions  The options to be modified.
   * @returns {!TestEntryInfo}
   */
  cloneWith(newOptions) {
    return new TestEntryInfo(/** @type {TestEntryInfoOptions} */ (
        Object.assign({}, this, newOptions)));
  }

  /**
   * Clone the existing TestEntryInfo object to a new TestEntryInfo object but
   * with modified lastModifiedTime field. This is especially useful for
   * constructing TestEntryInfo for Recents view.
   *
   * @param {string} newDate the new modified date time
   * @return {!TestEntryInfo}
   */
  cloneWithModifiedDate(newDate) {
    return this.cloneWith({lastModifiedTime: newDate});
  }

  /**
   * Clone the existing TestEntryInfo object to a new TestEntryInfo object but
   * with modified targetPath field. This is especially useful for testing
   * rename functionality.
   *
   * @param {string} newName the new modified name
   * @return {!TestEntryInfo}
   */
  cloneWithNewName(newName) {
    return this.cloneWith({
      targetPath: newName,
      nameText: newName,
    });
  }
}

/**
 * Filesystem entries used by the test cases.
 * TODO(sashab): Rename 'nameText', 'sizeText' and 'typeText' to
 * 'expectedNameText', 'expectedSizeText' and 'expectedTypeText' to reflect that
 * they are the expected values for those columns in the file manager.
 *
 * @type {!Object<string, TestEntryInfo>}
 * @const
 */
export const ENTRIES = {
  hello: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'hello.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'hello.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  world: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.ogv',
    thumbnailFileName: 'image.png',
    targetPath: 'world.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'world.ogv',
    sizeText: '59 KB',
    typeText: 'OGG video',
  }),

  webm: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'world.webm',
    targetPath: 'world.webm',
    mimeType: 'video/webm',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'world.webm',
    sizeText: '17 KB',
    typeText: 'WebM video',
  }),

  video: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video_long.ogv',
    targetPath: 'video_long.ogv',
    mimeType: 'video/ogg',
    lastModifiedTime: 'Jan 14, 2019, 16:01 PM',
    nameText: 'video_long.ogv',
    sizeText: '166 KB',
    typeText: 'OGG video',
  }),

  subtitle: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.vtt',
    targetPath: 'world.vtt',
    mimeType: 'text/vtt',
    lastModifiedTime: 'Feb 7, 2019, 15:03 PM',
    nameText: 'world.vtt',
    sizeText: '46 bytes',
    typeText: 'VTT text',
  }),

  unsupported: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'random.bin',
    targetPath: 'unsupported.foo',
    mimeType: 'application/x-foo',
    lastModifiedTime: 'Jul 4, 2012, 10:36 AM',
    nameText: 'unsupported.foo',
    sizeText: '8 KB',
    typeText: 'FOO file',
  }),

  desktop: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image.png',
    thumbnailFileName: 'image.png',
    targetPath: 'My Desktop Background.png',
    mimeType: 'image/png',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'My Desktop Background.png',
    sizeText: '272 bytes',
    typeText: 'PNG image',
  }),

  image2: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image2.png',
    // No file extension.
    targetPath: 'image2',
    mimeType: 'image/png',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'image2',
    sizeText: '4 KB',
    typeText: 'PNG image',
  }),

  image3: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image3.jpg',
    targetPath: 'image3.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'image3.jpg',
    sizeText: '3 KB',
    typeText: 'JPEG image',
  }),

  smallJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'small.jpg',
    targetPath: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'small.jpg',
    sizeText: '1 KB',
    typeText: 'JPEG image',
  }),

  // Used to differentiate between .jpg and .jpeg handling.
  sampleJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'small.jpg',
    targetPath: 'sample.jpeg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'sample.jpeg',
    sizeText: '1 KB',
    typeText: 'JPEG image',
  }),

  brokenJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'broken.jpg',
    targetPath: 'broken.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'broken.jpg',
    sizeText: '1 byte',
    typeText: 'JPEG image',
  }),

  exifImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'exif.jpg',
    // No mime type.
    targetPath: 'exif.jpg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'exif.jpg',
    sizeText: '31 KB',
    typeText: 'JPEG image',
  }),

  webpImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image.webp',
    // No mime type.
    targetPath: 'image.webp',
    lastModifiedTime: 'Jan 19, 2021, 1:10 PM',
    nameText: 'image.webp',
    sizeText: '5 KB',
    typeText: 'WebP image',
  }),

  rawImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'raw.orf',
    // No mime type.
    targetPath: 'raw.orf',
    lastModifiedTime: 'May 20, 2019, 10:10 AM',
    nameText: 'raw.orf',
    sizeText: '214 KB',
    typeText: 'ORF image',
  }),

  nefImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'raw.nef',
    // No mime type.
    targetPath: 'raw.nef',
    lastModifiedTime: 'May 9, 2015, 11:16 PM',
    nameText: 'raw.nef',
    sizeText: '92 KB',
    typeText: 'NEF image',
  }),

  beautiful: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
    // No mime type.
    targetPath: 'Beautiful Song.ogg',
    lastModifiedTime: 'Nov 12, 2086, 12:00 PM',
    nameText: 'Beautiful Song.ogg',
    sizeText: '14 KB',
    typeText: 'OGG audio',
  }),

  movFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'video.mov',
    targetPath: 'mac.mov',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'mac.mov',
    sizeText: '875 bytes',
    typeText: 'QuickTime video',
  }),

  docxFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.docx',
    targetPath: 'word.docx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.wordprocessingml.document',
    lastModifiedTime: 'Jul 4, 2038, 10:35 AM',
    nameText: 'word.docx',
    sizeText: '9 KB',
    typeText: 'Word document',
  }),

  photos: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'photos',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: 'photos',
    sizeText: '--',
    typeText: 'Folder',
  }),

  testCSEDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Test Encrypted Document',
    mimeType: 'application/vnd.google-gsuite.encrypted; ' +
        'content="application/vnd.google-apps.document"',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'Test Encrypted Document.gdoc',
    sizeText: '--',
    typeText: 'Google document',
  }),

  testCSEFile: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'test-encrypted.txt',
    mimeType: 'application/vnd.google-gsuite.encrypted; content="text/plain"',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'test-encrypted.txt',
    sizeText: '--',
    typeText: 'Plain text',
    availableOffline: true,
  }),

  testDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Test Document',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'Test Document.gdoc',
    sizeText: '--',
    typeText: 'Google document',
  }),

  testSharedDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Test Shared Document',
    mimeType: 'application/vnd.google-apps.document',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Test Shared Document.gdoc',
    sizeText: '--',
    typeText: 'Google document',
  }),

  testSharedFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'test.txt',
    mimeType: 'text/plain',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Mar 20, 2012, 11:40 PM',
    nameText: 'test.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    pinned: true,
  }),

  sharedDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Shared',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Shared',
    sizeText: '--',
    typeText: 'Folder',
  }),

  sharedDirectoryFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Shared/file.txt',
    mimeType: 'text/plain',
    sharedOption: SharedOption.SHARED,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'file.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  newlyAdded: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
    targetPath: 'newly added file.ogg',
    mimeType: 'audio/ogg',
    lastModifiedTime: 'Sep 4, 1998, 12:00 AM',
    nameText: 'newly added file.ogg',
    sizeText: '14 KB',
    typeText: 'OGG audio',
  }),

  tallText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.txt',
    targetPath: 'tall.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.txt',
    sizeText: '546 bytes',
    typeText: 'Plain text',
  }),

  plainText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'plaintext',
    // No mime type, no file extension.
    targetPath: 'plaintext',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'plaintext',
    sizeText: '32 bytes',
    typeText: 'Plain text',
  }),

  utf8Text: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'utf8.txt',
    targetPath: 'utf8.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'utf8.txt',
    sizeText: '191 bytes',
    typeText: 'Plain text',
  }),

  mHtml: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'page.mhtml',
    targetPath: 'page.mhtml',
    mimeType: 'multipart/related',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'page.mhtml',
    sizeText: '421 bytes',
    typeText: 'HTML document',
  }),

  tallHtml: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.html',
    targetPath: 'tall.html',
    mimeType: 'text/html',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.html',
    sizeText: '589 bytes',
    typeText: 'HTML document',
  }),

  tallPdf: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tall.pdf',
    targetPath: 'tall.pdf',
    mimeType: 'application/pdf',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'tall.pdf',
    sizeText: '15 KB',
    typeText: 'PDF document',
  }),

  popupPdf: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'popup.pdf',
    targetPath: 'popup.pdf',
    mimeType: 'application/pdf',
    lastModifiedTime: 'Jul 4, 2000, 10:42 AM',
    nameText: 'popup.pdf',
    sizeText: '538 bytes',
    typeText: 'PDF document',
  }),

  imgPdf: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'img.pdf',
    targetPath: 'imgpdf',
    mimeType: 'application/pdf',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'imgpdf',
    sizeText: '1608 bytes',
    typeText: 'PDF document',
  }),

  smallDocx: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.docx',
    targetPath: 'text.docx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.wordprocessingml.document',
    lastModifiedTime: 'Jan 4, 2019, 10:57 AM',
    nameText: 'text.docx',
    sizeText: '8.7 KB',
    typeText: 'Office document',
    alternateUrl: 'https://drive.google.com/open?id=smalldocxid&usp=drive_fs',
  }),

  smallDocxHosted: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.docx',
    targetPath: 'synced.docx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.wordprocessingml.document',
    lastModifiedTime: 'Jan 4, 2019, 10:57 AM',
    nameText: 'synced.docx',
    sizeText: '8.7 KB',
    typeText: 'Office document',
    alternateUrl: 'https://docs.google.com/document/d/smalldocxid' +
        '?rtpof=true&usp=drive_fs',
  }),

  smallDocxPinned: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.docx',
    targetPath: 'pinned.docx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.wordprocessingml.document',
    lastModifiedTime: 'Jan 4, 2019, 10:57 AM',
    nameText: 'pinned.docx',
    sizeText: '8.7 KB',
    typeText: 'Office document',
    pinned: true,
    alternateUrl: 'https://docs.google.com/document/d/pinneddocxid' +
        '?rtpof=true&usp=drive_fs',
  }),

  smallXlsxPinned: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'sheet.xlsx',
    targetPath: 'pinned.xlsx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.spreadsheetml.sheet',
    lastModifiedTime: 'Jan 10, 2020, 11:58 PM',
    nameText: 'pinned.xlsx',
    sizeText: '5.7 KB',
    typeText: 'Office spreadsheet',
    pinned: true,
    alternateUrl: 'https://docs.google.com/document/d/pinnedxlsxid' +
        '?rtpof=true&usp=drive_fs',
  }),

  smallPptxPinned: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'presentation.pptx',
    targetPath: 'pinned.pptx',
    mimeType: 'application/vnd.openxmlformats-officedocument' +
        '.presentationml.presentation',
    lastModifiedTime: 'Jan 14, 2020, 10:15 AM',
    nameText: 'pinned.pptx',
    sizeText: '35.2 KB',
    typeText: 'Office document',
    pinned: true,
    alternateUrl: 'https://docs.google.com/document/d/pinnedpptxid' +
        '?rtpof=true&usp=drive_fs',
  }),

  pinned: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'pinned.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'pinned.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    pinned: true,
  }),

  directoryA: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder',
  }),

  directoryB: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'B',
    sizeText: '--',
    typeText: 'Folder',
  }),

  directoryC: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A/B/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'C',
    sizeText: '--',
    typeText: 'Folder',
  }),

  directoryD: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'D',
    sizeText: '--',
    typeText: 'Folder',
  }),

  directoryE: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D/E',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'E',
    sizeText: '--',
    typeText: 'Folder',
  }),

  directoryF: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'D/E/F',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'F',
    sizeText: '--',
    typeText: 'Folder',
  }),

  dotTrash: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.Trash',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: '.Trash',
    sizeText: '--',
    typeText: 'Folder',
  }),

  deeplyBurriedSmallJpeg: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'A/B/C/deep.jpg',
    sourceFileName: 'small.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'deep.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  linkGtoB: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'G',
    sourceFileName: 'A/B',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'G',
    sizeText: '--',
    typeText: 'Folder',
  }),

  linkHtoFile: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'H.jpg',
    sourceFileName: 'A/B/C/deep.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'H.jpg',
    sizeText: '886 bytes',
    typeText: 'JPEG image',
  }),

  linkTtoTransitiveDirectory: new TestEntryInfo({
    type: EntryType.LINK,
    targetPath: 'T',
    sourceFileName: 'G/C',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'T',
    sizeText: '--',
    typeText: 'Folder',
  }),

  zipArchive: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.zip',
    targetPath: 'archive.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'archive.zip',
    sizeText: '743 bytes',
    typeText: 'ZIP archive',
  }),

  zipSJISArchive: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'sjis.zip',
    targetPath: 'sjis.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Apr 6, 2022, 1:00 AM',
    nameText: 'sjis.zip',
    sizeText: '479 bytes',
    typeText: 'ZIP archive',
  }),

  zipExtArchive: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'tera.zip',
    targetPath: 'tera.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Apr 6, 2022, 1:00 AM',
    nameText: 'tera.zip',
    sizeText: '250 bytes',
    typeText: 'ZIP archive',
  }),

  debPackage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'package.deb',
    targetPath: 'package.deb',
    mimeType: 'application/vnd.debian.binary-package',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'package.deb',
    sizeText: '724 bytes',
    typeText: 'DEB file',
  }),

  tiniFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.tar.gz',
    targetPath: 'test.tini',
    mimeType: 'application/gzip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'test.tini',
    sizeText: '439 bytes',
    typeText: 'Crostini image file',
  }),

  hiddenFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: '.hiddenfile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 30, 2014, 3:30 PM',
    nameText: '.hiddenfile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  // Team-drive entries.
  teamDriveA: new TestEntryInfo({
    type: EntryType.SHARED_DRIVE,
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: true,
      canShare: true,
    },
  }),

  teamDriveAFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'teamDriveAFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveAFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveADirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'teamDriveADirectory',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'teamDriveADirectory',
    sizeText: '--',
    typeText: 'Folder',
    teamDriveName: 'Team Drive A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: true,
      canShare: false,
    },
  }),

  teamDriveAHostedFile: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'teamDriveAHostedDoc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Apr 10, 2013, 4:20 PM',
    nameText: 'teamDriveAHostedDoc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    teamDriveName: 'Team Drive A',
  }),

  teamDriveB: new TestEntryInfo({
    type: EntryType.SHARED_DRIVE,
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveBFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'teamDriveBFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveBFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  teamDriveBDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'teamDriveBDirectory',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'teamDriveBDirectory',
    sizeText: '--',
    typeText: 'Folder',
    teamDriveName: 'Team Drive B',
    capabilities: {
      canCopy: true,
      canDelete: false,
      canRename: false,
      canAddChildren: false,
      canShare: true,
    },
  }),

  // Computer entries.
  computerA: new TestEntryInfo({
    type: EntryType.COMPUTER,
    computerName: 'Computer A',
    folderFeature: {
      isMachineRoot: true,
    },
  }),

  computerAFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'computerAFile.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'computerAFile.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    computerName: 'Computer A',
    capabilities: {
      canCopy: true,
      canDelete: true,
      canRename: true,
      canAddChildren: false,
      canShare: true,
    },
  }),

  computerAdirectoryA: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'A',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    computerName: 'Computer A',
    nameText: 'A',
    sizeText: '--',
    typeText: 'Folder',
  }),

  // Read-only and write-restricted entries.
  // TODO(sashab): Generate all combinations of capabilities inside the test, to
  // ensure maximum coverage.

  // A folder that can't be renamed or deleted or have children added, but can
  // be copied and shared.
  readOnlyFolder: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Read-Only Folder',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Read-Only Folder',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  // A google doc file that can't be renamed or deleted, but can be copied and
  // shared.
  readOnlyDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Read-Only Doc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Read-Only Doc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  // A google doc file that can't be renamed, deleted, copied or shared.
  readOnlyStrictDocument: new TestEntryInfo({
    type: EntryType.FILE,
    targetPath: 'Read-Only (Strict) Doc',
    mimeType: 'application/vnd.google-apps.document',
    lastModifiedTime: 'Mar 20, 2013, 10:40 PM',
    nameText: 'Read-Only (Strict) Doc.gdoc',
    sizeText: '--',
    typeText: 'Google document',
    capabilities: {
      canCopy: false,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: false,
    },
  }),

  // A regular file that can't be renamed or deleted, but can be copied and
  // shared.
  readOnlyFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image4.jpg',
    targetPath: 'Read-Only File.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
    nameText: 'Read-Only File.jpg',
    sizeText: '9 KB',
    typeText: 'JPEG image',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  // A ZIP file that can't be renamed or deleted, but can be copied and
  // shared.
  readOnlyZipFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'archive.zip',
    targetPath: 'archive.zip',
    mimeType: 'application/x-zip',
    lastModifiedTime: 'Jan 1, 2014, 1:00 AM',
    nameText: 'archive.zip',
    sharedOption: SharedOption.SHARED,
    sizeText: '743 bytes',
    typeText: 'ZIP archive',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  // A regular file that can't be renamed, but can be deleted.
  deletableFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Deletable File.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'Deletable File.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: false,
      canDelete: true,
    },
  }),

  // A regular file that can't be deleted, but can be renamed.
  renamableFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Renamable File.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'Renamable File.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    capabilities: {
      canCopy: true,
      canAddChildren: false,
      canRename: true,
      canDelete: false,
    },
  }),

  // Default Android directories.
  directoryDocuments: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Documents',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Documents',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  directoryMovies: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Movies',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Movies',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  directoryMusic: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Music',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Music',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  directoryPictures: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Pictures',
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Pictures',
    sizeText: '--',
    typeText: 'Folder',
    capabilities: {
      canCopy: false,
      canAddChildren: true,
      canRename: false,
      canDelete: false,
      canShare: true,
    },
  }),

  // Android test files.
  documentsText: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Documents/android.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'android.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  moviesVideo: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'world.webm',
    targetPath: 'Movies/android.webm',
    mimeType: 'video/webm',
    lastModifiedTime: 'Jul 4, 2012, 10:35 AM',
    nameText: 'android.webm',
    sizeText: '17 KB',
    typeText: 'WebM video',
  }),

  musicAudio: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'music.ogg',
    targetPath: 'Music/android.ogg',
    mimeType: 'audio/ogg',
    lastModifiedTime: 'Sep 4, 1998, 12:00 AM',
    nameText: 'android.ogg',
    sizeText: '14 KB',
    typeText: 'OGG audio',
  }),

  picturesImage: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'image3.jpg',
    targetPath: 'Pictures/android.jpg',
    mimeType: 'image/jpeg',
    lastModifiedTime: 'Jan 18, 2012, 1:02 AM',
    nameText: 'android.jpg',
    sizeText: '3 KB',
    typeText: 'JPEG image',
  }),

  neverSync: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'never-sync.txt',
    mimeType: 'text/plain',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'never-sync.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  sharedWithMeDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'Shared Directory',
    sharedOption: SharedOption.SHARED_WITH_ME,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'Shared Directory',
    sizeText: '--',
    typeText: 'Folder',
  }),

  sharedWithMeDirectoryFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'Shared Directory/file.txt',
    mimeType: 'text/plain',
    sharedOption: SharedOption.NESTED_SHARED_WITH_ME,
    lastModifiedTime: 'Jan 1, 2000, 1:00 AM',
    nameText: 'file.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  crdownload: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'hello.crdownload',
    mimeType: 'application/octet-stream',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: 'hello.crdownload',
    sizeText: '51 bytes',
    typeText: 'CRDOWNLOAD file',
  }),

  pluginVm: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'PvmDefault',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: 'Windows Files',
    sizeText: '--',
    typeText: 'Folder',
  }),

  invalidLastModifiedDate: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'text.txt',
    targetPath: 'invalidLastModifiedDate.txt',
    mimeType: 'text/plain',
    nameText: 'invalidLastModifiedDate.txt',
    sizeText: '51 bytes',
    typeText: 'Plain text',
  }),

  trashRootDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.Trash',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: '.Trash',
    sizeText: '--',
    typeText: 'Folder',
  }),

  trashInfoDirectory: new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.Trash/info',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    nameText: 'info',
    sizeText: '--',
    typeText: 'Folder',
  }),

  oldTrashInfoFile: new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'old_file.trashinfo',
    targetPath: '.Trash/info/hello.txt.trashinfo',
    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
    mimeType: 'text/plan',
    nameText: 'hello.txt.trashinfo',
    sizeText: '64 bytes',
    typeText: 'TRASHINFO',
  }),
};


/**
 * Creates a test file, which can be inside folders, however parent folders
 * have to be created by the caller using |createTestFolder|.
 * @param {string} path File path to be created,
 * @return {TestEntryInfo}
 */
export function createTestFile(path) {
  const name = path.split('/').pop();
  return new TestEntryInfo({
    targetPath: path,
    nameText: name,
    type: EntryType.FILE,
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    sizeText: '51 bytes',
    typeText: 'Plain text',
    sourceFileName: 'text.txt',
    mimeType: 'text/plain',
  });
}

/**
 * Returns the count for |value| for the histogram |name|.
 * @param {string} name The histogram to be queried.
 * @param {number} value The value within that histogram to query.
 * @return {!Promise<number>} A promise fulfilled with the count.
 */
export async function getHistogramCount(name, value) {
  const result = await sendTestMessage({
    'name': 'getHistogramCount',
    'histogramName': name,
    'value': value,
  });
  return /** @type {number} */ (JSON.parse(result));
}

/**
 * Returns the sum for for the histogram |name|.
 * @param {string} name The histogram to be queried.
 * @return {!Promise<number>} A promise fulfilled with the sum.
 */
export async function getHistogramSum(name) {
  const result = await sendTestMessage({
    'name': 'getHistogramSum',
    'histogramName': name,
  });
  return /** @type {number} */ (parseInt(JSON.parse(result), 10));
}

/**
 * Checks the expected total count for the histogram |name|.
 * @param {string} name The histogram to be queried.
 * @param {number} count The expected sample count.
 */
export async function expectHistogramTotalCount(name, count) {
  await sendTestMessage({
    'name': 'expectHistogramTotalCount',
    'histogramName': name,
    'count': count,
  });
}

/**
 * Returns the count for the user action |name|.
 * @param {string} name The user action to be queried.
 * @return {!Promise<number>} A promise fulfilled with the count.
 */
export async function getUserActionCount(name) {
  const result = await sendTestMessage({
    'name': 'getUserActionCount',
    'userActionName': name,
  });
  return /** @type {number} */ (JSON.parse(result));
}

/**
 * Returns a date time string with diff days. This can be used as the
 * lastModifiedTime field of TestEntryInfo object, which is useful to construct
 * a recent file.
 * @param {number} diffDays how many days in diff
 * @return {string}
 */
export function getDateWithDayDiff(diffDays) {
  const nowDate = new Date();
  nowDate.setDate(nowDate.getDate() - diffDays);
  // Format: "May 2, 2021, 11:25 AM"
  return formatDate(nowDate);
}

/**
 * Formats the date to be able to compare to Files app date.
 */
export function formatDate(date) {
  return sanitizeDate(date.toLocaleString('default', {
    month: 'short',
    day: 'numeric',
    year: 'numeric',
    hour12: true,
    hour: 'numeric',
    minute: 'numeric',
  }));
}

/**
 * Sanitizes the formatted date. Replaces unusual space with normal space.
 * @param {string} strDate the date already in the string format.
 * @return {string}
 */
export function sanitizeDate(strDate) {
  return strDate.replace('\u202f', ' ');
}
