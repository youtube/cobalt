// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../files_app_state.js';
import {addEntries, ENTRIES, EntryType, getCaller, getDateWithDayDiff, pending, repeatUntil, RootPath, sendTestMessage, SharedOption, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, remoteCall, setupAndWaitUntilReady} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET, NESTED_ENTRY_SET} from './test_data.js';

/**
 * @param {string} appId The ID that identifies the files app.
 * @param {string} type The search option type (location, recency, type).
 * @return {Promise<string>} The text of the element with 'selected-option' ID.
 */
async function getSelectedOptionText(appId, type) {
  // Force refresh of the element by showing the dropdown menu.
  await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [
    [
      'xf-search-options',
      `xf-select#${type}-selector`,
    ],
  ]);
  // Fetch the current selected item.
  const option = await remoteCall.waitForElement(appId, [
    'xf-search-options',
    `xf-select#${type}-selector`,
    '#selected-option',
  ]);
  return option.text;
}

/**
 * Tests searching inside Downloads with results.
 */
testcase.searchDownloadsWithResults = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Wait file list to display the search result.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));

  // Check that a11y message for results has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
};

/**
 * Tests searching inside Downloads without results.
 */
testcase.searchDownloadsWithNoResults = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for name not present among basic entry set.
  await remoteCall.typeSearchText(appId, 'INVALID TERM');

  // Wait file list to display no results.
  await remoteCall.waitForFiles(appId, []);

  // Check that a11y message for no results has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'There are no results for INVALID TERM.', a11yMessages[0]);
};

/**
 * Tests that clearing the search box announces the A11y.
 */
testcase.searchDownloadsClearSearch = async () => {
  // Perform a normal search, to be able to clear the search box.
  const appId = await testcase.searchDownloadsWithResults();

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Wait for the search box to fully collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Wait for file list to display all files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(
      2, a11yMessages.length,
      `Want 2 messages got ${JSON.stringify(a11yMessages)}`);
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);
};

/**
 * Tests that clearing the search box with keydown crbug.com/910068.
 */
testcase.searchDownloadsClearSearchKeyDown = async () => {
  // Perform a normal search, to be able to clear the search box.
  const appId = await testcase.searchDownloadsWithResults();

  const clearButton = '#search-box .clear';
  // Wait for clear button.
  await remoteCall.waitForElement(appId, clearButton);

  // Send a enter key to the clear button.
  const enterKey = [clearButton, 'Enter', false, false, false];
  await remoteCall.fakeKeyDown(appId, ...enterKey);

  // Check: Search input field is empty.
  const searchInput =
      await remoteCall.waitForElement(appId, '#search-box [type="search"]');
  chrome.test.assertEq('', searchInput.value);

  // Wait until the search button get the focus.
  // Use repeatUntil() here because the focus won't shift to search button
  // until the CSS animation is finished.
  const caller = getCaller();
  await repeatUntil(async () => {
    const activeElement =
        await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
    if (activeElement.attributes['id'] !== 'search-button') {
      return pending(
          caller, 'Expected active element should be search-button, got %s',
          activeElement.attributes['id']);
    }
  });
};

/**
 * Tests that the search text entry box stays expanded until the end of user
 * interaction.
 */
testcase.searchHidingTextEntryField = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select an entry in the file list.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Verify the toolbar search text entry box is enabled.
  let textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);

  // Send a 'mousedown' to the toolbar 'delete' button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#delete-button', 'mousedown']));

  // Verify the toolbar search text entry is still enabled.
  textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);

  // Send a 'mouseup' to the toolbar 'delete' button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#delete-button', 'mouseup']));

  // Verify the toolbar search text entry is still enabled.
  textInputElement =
      await remoteCall.waitForElement(appId, ['#search-box cr-input', 'input']);
  chrome.test.assertEq(undefined, textInputElement.attributes['disabled']);
};

/**
 * Tests that the search box collapses when empty and Tab out of the box.
 */
testcase.searchHidingViaTab = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Search box should start collapsed.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Verify the search input has focus.
  const input =
      await remoteCall.callRemoteTestUtil('deepGetActiveElement', appId, []);
  chrome.test.assertEq(input.attributes['id'], 'input');
  chrome.test.assertEq(input.attributes['aria-label'], 'Search');

  // Send Tab key to focus the next element.
  const result = await sendTestMessage({name: 'dispatchTabKey'});
  chrome.test.assertEq(result, 'tabKeyDispatched', 'Tab key dispatch failure');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
};

/**
 * Tests that clicking the search button expands and collapses the search box.
 */
testcase.searchButtonToggles = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Search box should start collapsed.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Measure the width of the search box when it's collapsed.
  const collapsedSearchBox = await remoteCall.waitForElementStyles(
      appId, '#search-wrapper', ['width']);

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Check: The search box width should have increased.
  const caller = getCaller();
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);
    if (collapsedSearchBox.renderedWidth >= element.renderedWidth) {
      return pending(caller, 'Waiting search box to expand');
    }
  });

  // Click the toolbar search button again.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Check: the search box should collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check: the search box width should decrease.
  await repeatUntil(async () => {
    const element = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);
    if (collapsedSearchBox.renderedWidth < element.renderedWidth) {
      return pending(caller, 'Waiting search box to collapse');
    }
  });
};

/**
 * Tests that Files app performs a search at app start up when
 * LaunchParam.searchQuery is specified.
 */
testcase.searchQueryLaunchParam = async () => {
  // Open Files app with LaunchParam.searchQuery='gdoc'.
  const query = 'gdoc';
  /** @type {!FilesAppState} */
  const appState = {searchQuery: query};
  const appId = await setupAndWaitUntilReady(
      null, BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET, appState);

  // Check: search box should be filled with the query.
  const caller = getCaller();
  await repeatUntil(async () => {
    const searchBoxInput =
        await remoteCall.waitForElement(appId, '#search-box cr-input');
    if (searchBoxInput.value !== query) {
      return pending(caller, 'Waiting search box to be filled with the query.');
    }
  });

  // Check: "My Drive" directory should be selected because it is the sole
  //        directory that contains query-matched files (*.gdoc).
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForSelectedItemByLabel('My Drive');
  await directoryTree.waitForFocusedItemByLabel('My Drive');

  // Check: Query-matched files should be shown in the files list.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.testDocument,
    ENTRIES.testSharedDocument,
  ]));
};

/**
 * Checks that changing location options correctly filters search results.
 */
testcase.searchWithLocationOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding nested directories and
  // a copy of the hello entry.
  const nestedHello = ENTRIES.hello.cloneWith({
    targetPath: 'A/hello.txt',
  });
  addEntries(['local'], [ENTRIES.directoryA, nestedHello]);

  // Start in the nested directory, as the default search location
  // is THIS_FOLDER. Expect to find one hello file. Then search on
  // THIS_CHROMEBOOK and expect to find two.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files/Downloads/A');

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Verify that the search options are visible.
  await remoteCall.waitForElement(appId, 'xf-search-options:not([hidden])');

  // Expect only the nested hello to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    nestedHello,
  ]));

  // Click the second button, which is This Chromebook.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "This Chromebook" location selector');

  // Expect all hello files to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    nestedHello,
  ]));
};

/**
 * Checks that changing recency options correctly filters search results.
 */
testcase.searchWithRecencyOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding another hello file with
  // a recent date. We cannot make it today's date as those dates
  // are rendered with 'Today' string rather than actual date string.
  const recentHello = ENTRIES.hello.cloneWith({
    nameText: 'hello-recent.txt',
    lastModifiedTime: new Date().toDateString(),
    targetPath: 'hello-recent.txt',
  });
  await addEntries(['local'], [recentHello]);
  // Unfortunately, today's files use custom date string. Make it so.
  const todayHello = recentHello.cloneWith({
    lastModifiedTime: 'Today 12:00 AM',
  });

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect two files, with no recency restrictions.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    todayHello,
  ]));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect only the recent hello file to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    todayHello,
  ]));
};

/**
 * Checks that when searching Google Drive we correctly match on name, not on
 * contents.
 */
testcase.matchDriveFilesByName = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, BASIC_LOCAL_ENTRY_SET, [
        ENTRIES.image2,
      ]);
  await remoteCall.typeSearchText(appId, 'image2');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.image2]));
};

/**
 * Checks that changing recency options correctly filters search results on
 * drive.
 */
testcase.searchDriveWithRecencyOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Modify the basic entry set by adding another hello file with
  // a recent date. We cannot make it today's date as those dates
  // are rendered with 'Today' string rather than actual date string.
  const recentHello = ENTRIES.hello.cloneWith({
    nameText: 'hello-recent.txt',
    lastModifiedTime: getDateWithDayDiff(3),
    targetPath: 'hello-recent.txt',
  });
  await addEntries(['drive'], [recentHello]);

  // Navigate to Google Drive. We are searching "local" directory, which limits
  // search results to Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My Drive');

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect two files, with no recency restrictions.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    recentHello,
  ]));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect only the recent hello file to be found.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    recentHello,
  ]));
};

/**
 * Checks that changing file types options correctly filters local
 * search results.
 */
testcase.searchLocalWithTypeOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'o');

  // Expect all basic files, with no type restrictions.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

  // Click the fifth button, which is "Video" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 5),
      'Failed to click "Videos" type selector');

  // Expect only world, which is a video file.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.world,
  ]));
};

/**
 * Checks that changing file types options correctly filters
 * Drive search results.
 */
testcase.searchDriveWithTypeOptions = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Navigate to Google Drive; make sure we have the desired files.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My Drive');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET));

  // Search the Drive for all files with "b" in their name.
  await remoteCall.typeSearchText(appId, 'b');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]));

  // Click the second button, which is "Audio" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 2),
      'Failed to click "Audio" type selector');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.beautiful,
  ]));
};

/**
 * @param {boolean} withPartitions Whether or not USB has partitions.
 * @return {string} The label that can be used to query for elements.
 */
function getUsbVolumeQuery(withPartitions) {
  return withPartitions ? 'Drive Label' : 'fake-usb';
}

/**
 * @param {string} appId The ID of the files app under test.
 * @param {boolean} withPartitions Whether or not USB has partitions.
 */
async function mountUsb(appId, withPartitions) {
  const nameSuffix = withPartitions ? 'UsbWithPartitions' : 'FakeUsb';
  await sendTestMessage({name: `mount${nameSuffix}`});
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemByLabel(getUsbVolumeQuery(withPartitions));
}

/**
 * Checks that the new search correctly finds files on a USB drive.
 */
testcase.searchRemovableDevice = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // Mount a USB with no partitions.
  await mountUsb(appId, false);

  // Navigate to the root of the USB.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.selectItemByLabel(getUsbVolumeQuery(false));

  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
};

/**
 * Checks that the new search correctly finds files on a USB drive with multiple
 * partitions.
 */
testcase.searchPartitionedRemovableDevice = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await mountUsb(appId, true);

  // Wait for removable partition-1 to appear in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  const partitionOne = await directoryTree.waitForItemByLabel('partition-1');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionOne));

  // Wait for removable partition-2 to appear in the directory tree.
  const partitionTwo = await directoryTree.waitForItemByLabel('partition-2');
  chrome.test.assertEq(
      'removable', directoryTree.getItemVolumeType(partitionTwo));

  // Navigate to the root of the USB.
  await directoryTree.selectItemByLabel(getUsbVolumeQuery(true));

  // Search for the 'hello' and expect two files; ignore the modified time
  // as these were copied when mounting the USB.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        ENTRIES.hello,
        ENTRIES.hello,
      ]),
      {ignoreLastModifiedTime: true});
};
/**
 * Checks that the search options are reset to default on folder change.
 */
testcase.resetSearchOptionsOnFolderChange = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Type something into the search query to see search options.
  await remoteCall.typeSearchText(appId, 'b');

  // Check the defaults.
  chrome.test.assertEq(
      'Downloads', await getSelectedOptionText(appId, 'location'));
  chrome.test.assertEq(
      'Any time', await getSelectedOptionText(appId, 'recency'));
  chrome.test.assertEq('All types', await getSelectedOptionText(appId, 'type'));

  // Change options.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 2),
      'Failed to change to "Audio" type selector');
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to change to "Last week" recency selector');

  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Start search again.
  await remoteCall.typeSearchText(appId, 'b');

  // Check that we are back to defaults.
  chrome.test.assertEq(
      'photos', await getSelectedOptionText(appId, 'location'));
  chrome.test.assertEq(
      'Any time', await getSelectedOptionText(appId, 'recency'));
  chrome.test.assertEq('All types', await getSelectedOptionText(appId, 'type'));
};

/**
 * Checks that we are showing the correct message in breadcrumbs when search is
 * active.
 */
testcase.showSearchResultMessageWhenSearching = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check that we start with My Files
  const beforeSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/My files/Downloads', beforeSearchPath);

  // Type something into the search query to start search.
  await remoteCall.typeSearchText(appId, 'b');

  // Wait for the search to fully expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Check that the breadcumb shows that we are searching.
  const duringSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/Search results', duringSearchPath);

  // Clear and close search.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Wait for the search to fully close.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Expect the path to return to the original path.
  const afterSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq(beforeSearchPath, afterSearchPath);
};

/**
 * Checks that search works correctly when starting in My Files.
 */
testcase.searchFromMyFiles = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files');
  const beforeSearchPath =
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
  chrome.test.assertEq('/My files', beforeSearchPath);

  // Make sure the search returns a matching file even when originating in My
  // Files rather than My Files/Downloads directory.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
  ]));

  // Close the search before adding Linux files.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Add Linux files.
  await mountCrostini(appId);
  // Add some Linux specific files.
  await addEntries(['crostini'], [ENTRIES.debPackage]);
  // Navigate back to /My files
  await directoryTree.navigateToPath('/My files');

  // Search for files containing ack (should include debPackage.
  await remoteCall.typeSearchText(appId, 'ack');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.desktop,
    ENTRIES.desktop,
    ENTRIES.debPackage,
  ]));
};

/**
 * Checks that the selection path correctly reflects paths of elements found by
 * search.
 */
testcase.selectionPath = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, NESTED_ENTRY_SET.concat([
        ENTRIES.hello,
        ENTRIES.desktop,
        ENTRIES.deeplyBurriedSmallJpeg,
      ]));
  // Search for files containing 'e'; should be three of those.
  await remoteCall.typeSearchText(appId, 'e');
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.deeplyBurriedSmallJpeg,
  ]));
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  const breadcrumbSingleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertFalse(breadcrumbSingleSelection.hidden);
  chrome.test.assertEq(
      'My files/Downloads/' + ENTRIES.hello.nameText,
      breadcrumbSingleSelection.attributes.path);
  // Select now the desktop entry, too. Two or more selected files,
  // regardless of the directory in which they sit, result in no path.
  await remoteCall.waitAndClickElement(
      appId, `#file-list [file-name="${ENTRIES.desktop.nameText}"]`,
      {ctrl: true});
  const breadcrumbDoubleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(breadcrumbDoubleSelection.hidden);
  chrome.test.assertEq('', breadcrumbDoubleSelection.attributes.path);
  await remoteCall.waitAndClickElement(
      appId,
      `#file-list [file-name="${ENTRIES.deeplyBurriedSmallJpeg.nameText}"]`,
      {ctrl: true});
  const breadcrumbTripleSelection = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(breadcrumbTripleSelection.hidden);
  chrome.test.assertEq('', breadcrumbTripleSelection.attributes.path);

  // Close search. Select any file. Confirm that the path display is not shown,
  // now that the search is inactive.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);
  const pathDisplayWhileNotSearching = await remoteCall.waitForElement(appId, [
    '#search-breadcrumb',
  ]);
  chrome.test.assertTrue(pathDisplayWhileNotSearching.hidden);
};

/**
 * Checks that we correctly traverse search hierarchy. If you start searching in
 * a local folder, the search should search it and its subfolders only. If we
 * change the location to be the root directory, it should correctly search any
 * folders (including Linux and Playfiles, if necessary) that are visually
 * under the root folder. Finally, search everywhere should search everything
 * we can search (Google Doc, removable drives, local file syste, * etc.).
 */
testcase.searchHierarchy = async () => {
  // hello file stored in My files/Downloads/photos.
  const photosHello = ENTRIES.hello.cloneWith({
    targetPath: 'photos/photos-hello.txt',
    nameText: 'photos-hello.txt',
  });
  // hello file stored in My files
  const myFilesHello = ENTRIES.hello.cloneWith({
    targetPath: 'my-files-hello.txt',
    nameText: 'my-files-hello.txt',
  });
  // hello file stored on Linux.
  const linuxHello = ENTRIES.hello.cloneWith({
    targetPath: 'linux-hello.txt',
    nameText: 'linux-hello.txt',
  });
  // hello file stored on a removable drive.
  const usbHello = ENTRIES.hello.cloneWith({
    targetPath: 'usb-hello.txt',
    nameText: 'usb-hello.txt',
  });
  // hello file stored on Google Drive.
  const driveHello = ENTRIES.hello.cloneWith({
    targetPath: 'drive-hello.txt',
    nameText: 'drive-hello.txt',
  });
  // hello file stored in playfiles.
  const playfilesHello = ENTRIES.hello.cloneWith({
    targetPath: 'Documents/playfile-hello.txt',
    nameText: 'playfile-hello.txt',
  });

  // Set up the app. This creates entries in My files and Drive.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [myFilesHello, ENTRIES.photos, photosHello],
      [driveHello]);

  // Mount USBs.
  await mountUsb(appId, false);

  // Add Linux files.
  await mountCrostini(appId);

  // Add custom hello files to Linux, USB, and PlayFiles.
  await addEntries(['android_files'], BASIC_ANDROID_ENTRY_SET.concat([
    ENTRIES.directoryDocuments,
    playfilesHello,
  ]));
  await addEntries(['usb'], [usbHello]);
  await addEntries(['crostini'], [linuxHello]);

  // Move to a nested directory under My files.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files/Downloads/photos');

  // Expect photosHello, as the only result when searching in photos.
  await remoteCall.typeSearchText(appId, '-hello.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([photosHello]),
      {ignoreLastModifiedTime: true});

  // Select the second button, which is root directory (My Fles in our case).
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 2),
      'Failed to click "My files" location selector');

  // Expect files from My files, Play files and Linux files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        myFilesHello,
        photosHello,
        linuxHello,
        playfilesHello,
      ]),
      {ignoreLastModifiedTime: true});

  // Select the first button, which is Everywhere.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'location', 1),
      'Failed to click "Everywhere" location selector');

  // Expect files from My files, Play files, Linux files, USB, and Drive.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([
        myFilesHello,
        photosHello,
        linuxHello,
        playfilesHello,
        driveHello,
        usbHello,
      ]),
      {ignoreLastModifiedTime: true});
};

/**
 * Checks that search is not visible when in the Trash volume.
 */
testcase.hideSearchInTrash = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  // Make sure that the search button is visible in Downloads.
  await remoteCall.waitForElement(appId, '#search-button');
  let searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['display']);
  chrome.test.assertTrue(searchButton.styles['display'] !== 'none');

  // Navigate to Trash and confirm that the search button is now hidden.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/Trash');
  searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['visibility']);
  chrome.test.assertTrue(searchButton.styles['visibility'] === 'hidden');

  // Try to use keyboard shortcuts Ctrl+F to launch search anyway.
  const ctrlF = ['#file-list', 'f', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlF);

  const searchWrapper =
      await remoteCall.waitForElement(appId, ['#search-wrapper']);
  // Confirm that search wrapper is still in collapsed state.
  chrome.test.assertEq(searchWrapper.attributes.collapsed, '');

  // Go back to Downloads and confirm that the search button is visible again.
  await directoryTree.navigateToPath('/My files/Downloads');
  searchButton = await remoteCall.waitForElementStyles(
      appId, ['#search-button'], ['visibility']);
  chrome.test.assertTrue(searchButton.styles['visibility'] !== 'hidden');

  // Make sure that search still works.
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
};

/**
 * Checks that files in trash do not appear in the search results when trash
 * is enabled, and appear when it is disabled.
 */
testcase.searchTrashedFiles = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we can find hello.txt.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));

  // Clear and close search.
  await remoteCall.waitAndClickElement(appId, '#search-box .clear');

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');
  // Delete hello.txt and wait for it to be moved to trash.
  await remoteCall.clickTrashButton(appId);

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we cannot find it.
  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([]));

  // Disable trash.
  await sendTestMessage({name: 'setTrashEnabled', enabled: false});

  // Search for all files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');
  // Confirm that we can find it. We also expect trashinfo file to appear.
  const helloTrashinfo = ENTRIES.hello.cloneWith({
    nameText: 'hello.txt.trashinfo',
    typeText: 'TRASHINFO file',
  });
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello, helloTrashinfo]),
      {ignoreLastModifiedTime: true, ignoreFileSize: true});
};

/**
 * Checcks that finding files directly in Shared with me, or in folders nested
 * in Shared with me, works.
 */
testcase.searchSharedWithMe = async () => {
  // Create a shared file for nested directory. It must have SHARED_WITH_ME
  // attribute on it, as NESTED_SHARED_WITH_ME does not have shared metadata
  // set on it.
  const nestedTestSharedFile = ENTRIES.sharedWithMeDirectoryFile.cloneWith({
    sharedOption: SharedOption.SHARED_WITH_ME,
    targetPath: 'Shared Directory/nested.txt',
    nameText: 'nested.txt',
  });
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], [
    ENTRIES.testSharedFile,
    ENTRIES.sharedWithMeDirectory,
    nestedTestSharedFile,
  ]);

  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/Shared with me');

  // Find the specific file, test.txt
  await remoteCall.typeSearchText(appId, 'test.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.testSharedFile]));

  // Search for the file nested in the shared directory.
  await remoteCall.typeSearchText(appId, 'nested.txt');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([nestedTestSharedFile]));
};

/**
 * Checks that the simple search from the root of a documents provider directory
 * works. No file category or modified time filters are used.
 */
testcase.searchDocumentsProvider = async () => {
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);

  // Search for all files with "nam" in their name.
  await directoryTree.navigateToPath('/DocumentsProvider');
  await remoteCall.typeSearchText(appId, 'nam');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.renamableFile]),
      {ignoreLastModifiedTime: true});
};

/**
 * Checks that changing file types options correctly filters
 * files exposed via Documents Provider.
 */
testcase.searchDocumentsProviderWithTypeOptions = async () => {
  await addEntries(
      ['documents_provider'], COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET);
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);
  await directoryTree.navigateToPath('/DocumentsProvider');

  // Search the DocumentsProvider folder for files with "File" in their name.
  await remoteCall.typeSearchText(appId, 'File');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.readOnlyFile,
    ENTRIES.deletableFile,
    ENTRIES.renamableFile,
  ]));

  // Click the second button, which is "Images" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'type', 4),
      'Failed to click "Images" type selector');

  await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows([
    ENTRIES.readOnlyFile,
  ]));
};

/**
 * Checks that changing file types options correctly filters
 * files exposed via Documents Provider.
 */
testcase.searchDocumentsProviderWithRecencyOptions = async () => {
  const recentHellos = [];
  for (let i = 0; i < 10; ++i) {
    recentHellos.push(ENTRIES.hello.cloneWith({
      nameText: `hello-recent-${i}.txt`,
      lastModifiedTime: getDateWithDayDiff(4 - (i % 3)),
      targetPath: `hello-recent-${i}.txt`,
    }));
  }
  await addEntries(
      ['documents_provider'],
      COMPLEX_DOCUMENTS_PROVIDER_ENTRY_SET.concat(recentHellos));

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Wait for DocumentsProvider to mount and Verify that the files are visible.
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemToHaveChildrenByType(
      'documents_provider', /* hasChildren= */ true);
  await directoryTree.navigateToPath('/DocumentsProvider');

  // Search the DocumentsProvider for files with "hello" in their name.
  await remoteCall.typeSearchText(appId, 'hello');

  // Expect the original hello and recent hello files to be present.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows(recentHellos.concat([ENTRIES.hello])));

  // Click the fourth button, which is "Last week" option.
  chrome.test.assertTrue(
      !!await remoteCall.selectSearchOption(appId, 'recency', 4),
      'Failed to click "Last week" recency selector');

  // Expect all rececent hello files to be present.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(recentHellos));
};

/**
 * Checks that search works on volumes mounted via fileSystemProvider.
 */
testcase.searchFileSystemProvider = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await sendTestMessage({
    name: 'launchProviderExtension',
    manifest: 'manifest_source_device.json',
  });
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.waitForItemByType('provided');
  await directoryTree.navigateToPath('/Test (1)');
  await directoryTree.waitForFocusedItemByType('provided');
  await remoteCall.typeSearchText(appId, 'folder');
  const expectedFolder = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: 'folder',
    lastModifiedTime: 'Jan 1, 2000, 1:00 PM',
    nameText: 'folder',
    sizeText: '--',
    typeText: 'Folder',
  });
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([expectedFolder]),
      {ignoreLastModifiedTime: true});
};

/**
 * Test searching images by content. There are two modes supported: search by
 * text contained in the image and search by keywords associtated with
 * objects detected in the image. The first search is known as optical character
 * recorgnition (OCR), the second as image content annotation (ICA). However,
 * from the Files app point of view there is no difference and all it knows is
 * that there are "terms" associated with images processed by the local image
 * search service. So that's all we test: that we can find images by terms
 * associated with them.
 */
testcase.searchImageByContent = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello, ENTRIES.desktop, ENTRIES.image3]);

  // Pretend that the desktop image was processed by the local search service
  // and was assigned the keywords 'marsupial' and 'duck'.
  await sendTestMessage({
    name: 'setupImageTerms',
    path: ENTRIES.desktop.targetPath,
    terms: 'marsupial,duck',
  });
  // The second image, image2, had 'ghost' assigned to it.
  await sendTestMessage({
    name: 'setupImageTerms',
    path: ENTRIES.image3.targetPath,
    terms: 'ghost',
  });

  await remoteCall.typeSearchText(appId, 'marsupial');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.desktop]),
      {ignoreLastModifiedTime: true});

  // Search again, using the second term 'duck'.
  await remoteCall.typeSearchText(appId, 'duck');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.desktop]),
      {ignoreLastModifiedTime: true});

  // Search, using the term 'ghost', assigned to image3.
  await remoteCall.typeSearchText(appId, 'ghost');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.image3]),
      {ignoreLastModifiedTime: true});
};

/**
 * Checks that any search, regardless if it has results or not, is closed if we
 * navigate to another directory.
 */
testcase.changingDirectoryClosesSearch = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await remoteCall.typeSearchText(appId, 'hello');
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]));
  const directoryTree = await DirectoryTreePageObject.create(appId, remoteCall);
  await directoryTree.navigateToPath('/My files/Downloads/photos');
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');
};
