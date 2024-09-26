// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_DRIVE_ENTRY_SET, SHARED_DRIVE_ENTRY_SET} from './test_data.js';

/**
 * Test 'Share with others' for a file or directory on Drive.
 *
 * @param {!string} path Path of the file or directory to be shared.
 * @param {!string} url Expected URL for the browser to visit.
 * @param {!string|undefined} teamDrive If set, the team drive to switch to.
 */
async function shareWithOthersExpectBrowserURL(
    path, url, teamDrive = undefined) {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat(SHARED_DRIVE_ENTRY_SET));

  // Navigate to the specified team drive if one is specified.
  if (teamDrive !== undefined) {
    await navigateWithDirectoryTree(
        appId,
        teamDrive === '' ? '/Shared drives' : `/Shared drives/${teamDrive}`);

    // Wait for the file list to update.
    await remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length);
  }

  // Select the given |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Right-click to show the context menu.
  chrome.test.assertTrue(
      !!await remoteCall.waitAndRightClick(appId, '.table-row[selected]'));

  // Wait and click on the visible and enabled menu item.
  const shareMenuItem = '#file-context-menu:not([hidden]) ' +
      ' [command="#share"]:not([hidden]):not([disabled])';
  chrome.test.assertTrue(
      !!await remoteCall.waitAndClickElement(appId, shareMenuItem));

  // Wait for the context menu to disappear.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '#file-context-menu[hidden]'));

  // Wait for the browser window to appear and navigate to the expected URL.
  chrome.test.assertEq(
      url, await remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, []));
}

/**
 * Test 'Manage in Drive' for a file or directory on Drive.
 *
 * @param {!string} path Path of the file or directory to be managed.
 * @param {!string} url Expected URL for the browser to visit.
 * @param {!string|undefined} teamDrive If set, the team drive to switch to.
 */
async function manageWithDriveExpectBrowserURL(
    path, url, teamDrive = undefined) {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat(SHARED_DRIVE_ENTRY_SET));

  // Navigate to the specified team drive if one is specified.
  if (teamDrive !== undefined) {
    await navigateWithDirectoryTree(
        appId,
        teamDrive === '' ? '/Shared drives' : `/Shared drives/${teamDrive}`);

    // Wait for the file list to update.
    await remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length);
  }

  // Select the given |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the entry to be selected.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '.table-row[selected]'));

  // Right-click the selected entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseClick failed');

  // Wait for the context menu to appear.
  chrome.test.assertTrue(!!await remoteCall.waitForElement(
      appId, '#file-context-menu:not([hidden])'));

  // Click the "Manage in Drive" menu item.
  await remoteCall.waitAndClickElement(
      appId, '[command="#manage-in-drive"]:not([hidden]):not([disabled])');

  // Wait for the context menu to disappear.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '#file-context-menu[hidden]'));

  // Wait for the browser window to appear and navigate to the expected URL.
  chrome.test.assertEq(
      url, await remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, []));
}

/**
 * Tests sharing a file on Drive.
 */
testcase.shareFileDrive = () => {
  const URL = 'https://file_alternate_link/world.ogv?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL('world.ogv', URL);
};

/**
 * Tests sharing a directory on Drive.
 */
testcase.shareDirectoryDrive = () => {
  const URL = 'https://folder_alternate_link/photos?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL('photos', URL);
};

/**
 * Tests sharing a hosted file (gdoc) on Drive.
 */
testcase.shareHostedFileDrive = () => {
  const URL =
      'https://document_alternate_link/Test%20Document?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL('Test Document.gdoc', URL);
};

/**
 * Tests managing a file on Drive.
 */
testcase.manageFileDrive = () => {
  const URL = 'https://file_alternate_link/world.ogv';
  return manageWithDriveExpectBrowserURL('world.ogv', URL);
};

/**
 * Tests managing a directory on Drive.
 */
testcase.manageDirectoryDrive = () => {
  const URL = 'https://folder_alternate_link/photos';
  return manageWithDriveExpectBrowserURL('photos', URL);
};

/**
 * Tests managing a hosted file (gdoc) on Drive.
 */
testcase.manageHostedFileDrive = () => {
  const URL = 'https://document_alternate_link/Test%20Document';
  return manageWithDriveExpectBrowserURL('Test Document.gdoc', URL);
};

/**
 * Tests sharing a file in a team drive.
 */
testcase.shareFileTeamDrive = () => {
  const URL =
      'https://file_alternate_link/teamDriveAFile.txt?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL(
      'teamDriveAFile.txt', URL, 'Team Drive A');
};

/**
 * Tests that sharing a directory in a team drive is not allowed.
 */
testcase.shareDirectoryTeamDrive = async () => {
  const teamDrive = 'Team Drive A';
  const path = 'teamDriveADirectory';

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat(SHARED_DRIVE_ENTRY_SET));

  // Navigate to the team drive.
  await navigateWithDirectoryTree(appId, `/Shared drives/${teamDrive}`);

  // Wait for the file list to update.
  await remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length);

  // Select the given |path|.
  await remoteCall.waitUntilSelected(appId, path);

  // Wait for the entry to be selected.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElement(appId, '.table-row[selected]'));

  // Right-click the selected entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseClick failed');

  // Wait for the context menu to appear.
  chrome.test.assertTrue(!!await remoteCall.waitForElement(
      appId, '#file-context-menu:not([hidden])'));

  // Wait for the "Share" menu item to appear.
  await remoteCall.waitForElement(
      appId, '[command="#share"]:not([hidden])[disabled]');
};

/**
 * Tests sharing a hosted file (gdoc) in a team drive.
 */
testcase.shareHostedFileTeamDrive = () => {
  const URL =
      'https://document_alternate_link/teamDriveAHostedDoc?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL(
      'teamDriveAHostedDoc.gdoc', URL, 'Team Drive A');
};

/**
 * Tests managing a file in a team drive.
 */
testcase.manageFileTeamDrive = () => {
  const URL = 'https://file_alternate_link/teamDriveAFile.txt';
  return manageWithDriveExpectBrowserURL(
      'teamDriveAFile.txt', URL, 'Team Drive A');
};

/**
 * Tests managing a directory in a team drive.
 */
testcase.manageDirectoryTeamDrive = () => {
  const URL = 'https://folder_alternate_link/teamDriveADirectory';
  return manageWithDriveExpectBrowserURL(
      'teamDriveADirectory', URL, 'Team Drive A');
};

/**
 * Tests managing a hosted file (gdoc) in a team drive.
 */
testcase.manageHostedFileTeamDrive = () => {
  const URL = 'https://document_alternate_link/teamDriveAHostedDoc';
  return manageWithDriveExpectBrowserURL(
      'teamDriveAHostedDoc.gdoc', URL, 'Team Drive A');
};

/**
 * Tests managing a team drive.
 */
testcase.manageTeamDrive = () => {
  const URL = 'https://folder_alternate_link/Team%20Drive%20A';
  return manageWithDriveExpectBrowserURL('Team Drive A', URL, '');
};

/**
 * Tests sharing a team drive.
 */
testcase.shareTeamDrive = () => {
  const URL =
      'https://folder_alternate_link/Team%20Drive%20A?userstoinvite=%22%22';
  return shareWithOthersExpectBrowserURL('Team Drive A', URL, '');
};
