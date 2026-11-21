// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function() {
  const host = document.getElementById('host');
  const shadowRoot = chrome.dom.openOrClosedShadowRoot(host);
  chrome.test.assertEq(shadowRoot.childElementCount, 2);
  // Verify `chrome.dom.openOrClosedShadowRoot` throws when passed a
  // non-HTMLElement object.
  chrome.test.assertThrows(
      chrome.dom.openOrClosedShadowRoot.bind(null, /* element */ new Object()),
      /* expectedError */
      'Error in invocation of dom.openOrClosedShadowRoot(HTMLElement ' +
          'element): ');
  chrome.test.succeed();
}]);
