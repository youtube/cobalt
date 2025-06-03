// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.ReadAloud_KeyboardForPlayPause

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  let result = true;
  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  // The page needs some text to start speaking
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 3],
      },
      {
        id: 2,
        role: 'staticText',
        name: 'This is some text.',
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is some more text.',
      },
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2, 3]);

  const readAnythingApp = document.querySelector('read-anything-app');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const playPauseButton = toolbar.getElementById('play-pause');
  const dispatchTarget =
      readAnythingApp.shadowRoot.querySelector('#flex-parent');
  const keyK = new KeyboardEvent('keydown', {key: 'k'});

  // Unpause by pressing k
  dispatchTarget.dispatchEvent(keyK);
  assertEquals(
      playPauseButton.getAttribute('iron-icon'), 'read-anything-20:pause');
  assertEquals(readAnythingApp.paused, false);

  // Pause again by clicking k
  dispatchTarget.dispatchEvent(keyK);
  assertEquals(
      playPauseButton.getAttribute('iron-icon'), 'read-anything-20:play');
  assertEquals(readAnythingApp.paused, true);

  return result;
})();
