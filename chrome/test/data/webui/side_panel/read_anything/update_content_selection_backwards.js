// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateContent_Selection_Backwards

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');
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

  const assertContainerInnerHTML = (expected) => {
    const actual = container.innerHTML;
    assertEquals(actual, expected);
  };

  const setOnSelectionChangeForTest = () => {
    // This is called by readAnythingApp onselectionchange. It is usually
    // implemented by ReadAnythingAppController which forwards these
    // arguments to the browser process in the form of an
    // AXEventNotificationDetail. Instead, we capture the arguments here and
    // verify their values. Since onselectionchange is called
    // asynchronously, the test must wait for this function to be called;
    // therefore we fire a custom event on-selection-change-for-text here
    // for the test to await.
    chrome.readingMode.onSelectionChange =
        (anchorNodeId, anchorOffset, focusNodeId, focusOffset) => {
          readAnythingApp.dispatchEvent(
              new CustomEvent('on-selection-change-for-test', {
                detail: {
                  anchorNodeId: anchorNodeId,
                  anchorOffset: anchorOffset,
                  focusNodeId: focusNodeId,
                  focusOffset: focusOffset,
                },
              }));
        };
  };

  // root htmlTag='#document' id=1
  // ++paragraph htmlTag='p' id=2
  // ++++staticText name='Hello' id=3
  // ++paragraph htmlTag='p' id=4
  // ++++staticText name='World' id=5
  // ++paragraph htmlTag='p' id=6
  // ++++staticText name='Friend' id=7
  // ++++staticText name='!' id=8
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4, 6],
      },
      {
        id: 2,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'Hello',
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'World',
      },
      {
        id: 6,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [7, 8],
      },
      {
        id: 7,
        role: 'staticText',
        name: 'Friend',
      },
      {
        id: 8,
        role: 'staticText',
        name: '!',
      },
    ],
    selection: {
      anchor_object_id: 7,
      focus_object_id: 3,
      anchor_offset: 2,
      focus_offset: 1,
      is_backward: true,
    },
  };
  setOnSelectionChangeForTest();
  chrome.readingMode.setContentForTesting(axTree, []);
  // The expected string contains the complete text of each node in the
  // selection.
  const expected = '<div><p>Hello</p><p>World</p><p>Friend!</p></div>';
  assertContainerInnerHTML(expected);
  const selection = readAnythingApp.getSelection();
  assertEquals(selection.anchorNode.textContent, 'Hello');
  assertEquals(selection.focusNode.textContent, 'Friend');
  assertEquals(selection.anchorOffset, 1);
  assertEquals(selection.focusOffset, 2);

  return result;
})();
