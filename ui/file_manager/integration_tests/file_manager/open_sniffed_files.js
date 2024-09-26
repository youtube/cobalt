// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening files in the browser using content sniffing.
 */

import {addEntries, ENTRIES, EntryType, getBrowserWindows, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {expandTreeItem, mountCrostini, navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady, waitForMediaApp} from './background.js';

/**
 * Tests opening a file with missing filename extension from Files app.
 *
 * @param {string} path Directory path (Downloads or Drive).
 * @param {Object<TestEntryInfo>} entry FileSystem entry to use.
 */
async function sniffedFileOpen(path, entry) {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [entry.targetPath],
    openType: 'launch',
  });
  // Open Files.App on |path|, add imgpdf to Downloads and Drive.
  const appId = await setupAndWaitUntilReady(path, [entry], [entry]);

  // Open the file from Files app.
  if (entry.mimeType === 'application/pdf') {
    // When SWA is enabled, Backlight is also enabled and becomes the default
    // handler for PDF files. So we have to use the "open with" option to open
    // in the browser.

    // Select the file.
    await remoteCall.waitUntilSelected(appId, entry.targetPath);

    // Right-click the selected file.
    await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

    // Wait for the file context menu to appear.
    await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
    await remoteCall.waitAndClickElement(
        appId,
        '#file-context-menu:not([hidden]) ' +
            ' [command="#open-with"]:not([hidden])');

    // Wait for the sub-menu to appear.
    await remoteCall.waitForElement(appId, '#tasks-menu:not([hidden])');

    // Wait for the sub-menu item "View".
    await remoteCall.waitAndClickElement(
        appId,
        '#tasks-menu:not([hidden]) [file-type-icon="pdf"]:not([hidden])');
  } else {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, [entry.targetPath]));
  }

  // The SWA window itself is detected by getBrowserWindows().
  const initialWindowCount = 1;
  // Wait for a new browser window to appear.
  const browserWindows = await getBrowserWindows(initialWindowCount);

  // Find the main (normal) browser window.
  let normalWindow = undefined;
  for (let i = 0; i < browserWindows.length; ++i) {
    if (browserWindows[i].type == 'normal') {
      normalWindow = browserWindows[i];
      break;
    }
  }
  // Check we have found a 'normal' browser window.
  chrome.test.assertTrue(normalWindow !== undefined);
  // Check we have only one tab opened from trying to open the file.
  const tabs = normalWindow.tabs;
  chrome.test.assertTrue(tabs.length === 1);
  // Get the url of the tab, which may still be pending.
  const url = tabs[0].pendingUrl || tabs[0].url;
  // Check the end of the URL matches the file we tried to open.
  const tail = url.replace(/.*\//, '');
  chrome.test.assertTrue(tail === entry.targetPath);
}

testcase.pdfOpenDownloads = () => {
  return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.imgPdf);
};

testcase.pdfOpenDrive = () => {
  return sniffedFileOpen(RootPath.DRIVE, ENTRIES.imgPdf);
};

testcase.textOpenDownloads = () => {
  return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.plainText);
};

testcase.textOpenDrive = () => {
  return sniffedFileOpen(RootPath.DRIVE, ENTRIES.plainText);
};
