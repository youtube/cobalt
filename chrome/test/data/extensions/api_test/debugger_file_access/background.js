// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkUrlsEqual(expected, actual) {
  // Note: Use new URL(...).href to compare in order to normalize the URL,
  // which is important if the path referenced a parent (as happens in the
  // file urls).
  chrome.test.assertEq(new URL(expected).href,
                       new URL(actual).href);
}

let openTab;

async function runNotAllowedTest(method, params, expectAllowed) {
  const NOT_ALLOWED = "Not allowed";
  const NOT_FOUND = '\'Browser.setDownloadBehavior\' wasn\'t found';
  const tab = await openTab(chrome.runtime.getURL('dummy.html'));
  const debuggee = {tabId: tab.id};
  chrome.debugger.attach(debuggee, '1.2', function() {
    chrome.test.assertNoLastError();
    chrome.debugger.sendCommand(debuggee, method, params, onResponse);

    function onResponse() {
      var message;
      try {
        message = JSON.parse(chrome.runtime.lastError.message).message;
      } catch (e) {
      }
      chrome.debugger.detach(debuggee, () => {
        const allowed = message !== NOT_ALLOWED && message !== NOT_FOUND;
        if (allowed === expectAllowed)
          chrome.test.succeed();
        else
          chrome.test.fail('' + message);
      });
    }
  });
}

(async () => {
  const config = await new Promise((resolve) => {
                   chrome.test.getConfig(resolve)
                 });
  const fileUrl = config.testDataDirectory + '/../body1.html';
  const expectFileAccess = !!config.customArg;

  ({ openTab } = await import('/_test_resources/test_util/tabs_util.js'));

  console.log(fileUrl);

  chrome.test.runTests([
    function verifyInitialState() {
      if (config.customArg)
        chrome.test.assertEq('enabled', config.customArg);
      chrome.extension.isAllowedFileSchemeAccess((allowed) => {
        chrome.test.assertEq(expectFileAccess, allowed);
        chrome.test.succeed();
      });
    },

    function testAttach() {
      openTab(fileUrl).then((tab) => {
        checkUrlsEqual(fileUrl, tab.url);
        const tabId = tab.id;
        chrome.debugger.attach({tabId: tabId}, '1.1', function() {
          if (expectFileAccess) {
            chrome.test.assertNoLastError();
            chrome.debugger.detach({tabId: tabId}, function() {
              chrome.test.assertNoLastError();
              chrome.test.succeed();
            });
          } else {
            chrome.test.assertLastError('Cannot attach to this target.');
            chrome.test.succeed();
          }
        });
      });
    },

    function testAttachAndNavigate() {
      const url = chrome.runtime.getURL('dummy.html');
      openTab(url).then((tab) => {
        checkUrlsEqual(url, tab.url);
        const tabId = tab.id;
        chrome.debugger.attach({tabId: tabId}, '1.1', function() {
          chrome.test.assertNoLastError();
          let responded = false;

          function onResponse() {
            responded = true;
            if (expectFileAccess) {
              chrome.test.assertNoLastError();
            } else {
              chrome.test.assertLastError(JSON.stringify({
                code: -32000,
                message: 'Navigating to local URL is not allowed'
              }));
            }
            chrome.tabs.remove(tabId);
          }

          function onDetach(from, reason) {
            console.warn('Detached');
            chrome.debugger.onDetach.removeListener(onDetach);
            chrome.test.assertTrue(responded);
            chrome.test.assertEq(tabId, from.tabId);
            chrome.test.assertEq('target_closed', reason);
            chrome.test.succeed();
          }

          chrome.debugger.onDetach.addListener(onDetach);
          chrome.debugger.sendCommand({tabId: tabId}, 'Page.navigate',
                                      {url: fileUrl}, onResponse);
        });
      });
    },

    // https://crbug.com/866426
    function setDownloadBehavior() {
      // We never allow to write local files.
      runNotAllowedTest('Browser.setDownloadBehavior', {behavior: 'allow'},
          false);
    },

    // https://crbug.com/805557
    function setFileInputFiles() {
      // We only allow extensions with explicit file access to read local files.
      runNotAllowedTest('DOM.setFileInputFiles', {nodeId: 1, files: []},
          expectFileAccess);
    },
  ]);
})();
