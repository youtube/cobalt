// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/*
 * Verify that sharing options for an encrypted files are limited to nearby
 * sharing (which only uses URLs and doesn't need to access file contents) and
 * drive-based sharing (which is managing permissions in Google Drive and also
 * doesn't need to access the contents). Other options, such as “Copy”, should
 * be removed.
 */
testcase.checkEncryptedSharesheetOptions = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.testCSEFile]);

  await remoteCall.showContextMenuFor(appId, ENTRIES.testCSEFile.nameText);

  const shareMenuItem =
      '#file-context-menu:not([hidden]) [command="#invoke-sharesheet"]:not([hidden]):not([disabled])';
  chrome.test.assertTrue(
      !!await remoteCall.waitAndClickElement(appId, shareMenuItem),
      'failed to click context menu item');

  const shareInfo = await sendTestMessage({name: 'getSharesheetInfo'});
  const shareTargetsList =
      /** @type {!Array<string>} */ (JSON.parse(shareInfo));

  for (const target of shareTargetsList) {
    chrome.test.assertTrue(
        target == 'Share with others' || target == 'Nearby Share',
        'unexpected share target: ' + target);
  }
};
