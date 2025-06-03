// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerPdfSidenavElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

function createSidenav(): ViewerPdfSidenavElement {
  document.body.innerHTML = '';
  const sidenav = document.createElement('viewer-pdf-sidenav');
  document.body.appendChild(sidenav);
  return sidenav;
}

// Unit tests for the viewer-pdf-sidenav element.
const tests = [
  /**
   * Test that the sidenav toggles among thumbnail, outline and attachment view.
   */
  async function testViewToggle() {
    const sidenav = createSidenav();

    // Add some dummy bookmarks and attachments so that all 3 tabs appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1, children: []},
      {title: 'Bar', page: 2, children: []},
    ];
    sidenav.attachments = [
      {name: 'attachment1', size: 10, readable: true},
      {name: 'attachment2', size: -1, readable: true},
    ];

    await waitAfterNextRender(sidenav);

    const icons = sidenav.shadowRoot!.querySelector('#icons')!;
    const content = sidenav.shadowRoot!.querySelector('#content')!;
    const buttons = sidenav.shadowRoot!.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(3, buttons.length);

    const thumbnailButton = buttons[0]!;
    const outlineButton = buttons[1]!;
    const attachmentButton = buttons[2]!;

    const thumbnailBar = content.querySelector('viewer-thumbnail-bar')!;
    const outline = content.querySelector('viewer-document-outline')!;
    const attachmentBar = content.querySelector('viewer-attachment-bar')!;

    // Verify the button types.
    chrome.test.assertEq(
        'pdf:thumbnails', thumbnailButton.getAttribute('iron-icon'));
    chrome.test.assertEq(
        'pdf:doc-outline', outlineButton.getAttribute('iron-icon'));
    chrome.test.assertEq(
        'pdf:attach-file', attachmentButton.getAttribute('iron-icon'));

    function assertThumbnailView() {
      chrome.test.assertTrue(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          outlineButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          attachmentButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'true', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', outlineButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', attachmentButton.getAttribute('aria-selected'));

      chrome.test.assertFalse(thumbnailBar.hidden);
      chrome.test.assertTrue(outline.hidden);
      chrome.test.assertTrue(attachmentBar.hidden);
    }

    function assertOutlineView() {
      chrome.test.assertFalse(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertTrue(
          outlineButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          attachmentButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'false', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq('true', outlineButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', attachmentButton.getAttribute('aria-selected'));

      chrome.test.assertTrue(thumbnailBar.hidden);
      chrome.test.assertFalse(outline.hidden);
      chrome.test.assertTrue(attachmentBar.hidden);
    }

    function assertAttachmentView() {
      chrome.test.assertFalse(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          outlineButton.parentElement!.classList.contains('selected'));
      chrome.test.assertTrue(
          attachmentButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'false', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', outlineButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'true', attachmentButton.getAttribute('aria-selected'));

      chrome.test.assertTrue(thumbnailBar.hidden);
      chrome.test.assertTrue(outline.hidden);
      chrome.test.assertFalse(attachmentBar.hidden);
    }

    // Sidebar starts on thumbnail view.
    assertThumbnailView();

    // Click on outline view.
    outlineButton.click();
    assertOutlineView();

    // Click on attachment view.
    attachmentButton.click();
    assertAttachmentView();

    // Return to thumbnail view.
    thumbnailButton.click();
    assertThumbnailView();

    // Arrow keys toggle through thumbnail, outline and attachment view.
    // Thumbnail -> Outline
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertOutlineView();

    // Outline -> Attachment
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertAttachmentView();

    // Attachment -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertThumbnailView();

    // Thumbnail -> Attachment
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertAttachmentView();

    // Attachment -> Outline
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertOutlineView();

    // Outline -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertThumbnailView();

    // Pressing arrow keys outside of icons shouldn't do anything.
    keyDownOn(content, 0, '', 'ArrowDown');
    assertThumbnailView();

    chrome.test.succeed();
  },

  /**
   * Test that the sidenav toggles between thumbnail and outline view.
   */
  async function testThumbnailOutlineViewToggle() {
    const sidenav = createSidenav();

    // Add some dummy bookmarks and attachments so that thumbnail and outline
    // tabs appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1, children: []},
      {title: 'Bar', page: 2, children: []},
    ];

    await waitAfterNextRender(sidenav);
    const icons = sidenav.shadowRoot!.querySelector('#icons')!;
    const content = sidenav.shadowRoot!.querySelector('#content')!;
    const buttons = sidenav.shadowRoot!.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(2, buttons.length);

    const thumbnailButton = buttons[0]!;
    const outlineButton = buttons[1]!;
    const thumbnailBar = content.querySelector('viewer-thumbnail-bar')!;
    const outline = content.querySelector('viewer-document-outline')!;

    // Verify the button types.
    chrome.test.assertEq(
        'pdf:thumbnails', thumbnailButton.getAttribute('iron-icon'));
    chrome.test.assertEq(
        'pdf:doc-outline', outlineButton.getAttribute('iron-icon'));

    function assertThumbnailView() {
      chrome.test.assertTrue(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          outlineButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'true', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', outlineButton.getAttribute('aria-selected'));

      chrome.test.assertFalse(thumbnailBar.hidden);
      chrome.test.assertTrue(outline.hidden);
    }

    function assertOutlineView() {
      chrome.test.assertFalse(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertTrue(
          outlineButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'false', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq('true', outlineButton.getAttribute('aria-selected'));

      chrome.test.assertTrue(thumbnailBar.hidden);
      chrome.test.assertFalse(outline.hidden);
    }

    // Sidebar starts on thumbnail view.
    assertThumbnailView();

    // Click on outline view.
    outlineButton.click();
    assertOutlineView();

    // Return to thumbnail view.
    thumbnailButton.click();
    assertThumbnailView();

    // Arrow keys toggle through thumbnail and outline view.

    // Thumbnail -> Outline
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertOutlineView();

    // Outline -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertThumbnailView();

    // Thumbnail -> Outline
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertOutlineView();

    // Outline -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertThumbnailView();

    chrome.test.succeed();
  },

  /**
   * Test that the sidenav toggles between thumbnail and attachment view.
   */
  async function testThumbnailAttachmentViewToggle() {
    const sidenav = createSidenav();
    sidenav.attachments = [
      {name: 'attachment1', size: 10, readable: true},
      {name: 'attachment2', size: -1, readable: true},
    ];

    await waitAfterNextRender(sidenav);

    const icons = sidenav.shadowRoot!.querySelector('#icons')!;
    const content = sidenav.shadowRoot!.querySelector('#content')!;
    const buttons = sidenav.shadowRoot!.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(2, buttons.length);

    const thumbnailButton = buttons[0]!;
    const attachmentButton = buttons[1]!;

    const thumbnailBar = content.querySelector('viewer-thumbnail-bar')!;
    const attachmentBar = content.querySelector('viewer-attachment-bar')!;

    // Verify the button types.
    chrome.test.assertEq(
        'pdf:thumbnails', thumbnailButton.getAttribute('iron-icon'));
    chrome.test.assertEq(
        'pdf:attach-file', attachmentButton.getAttribute('iron-icon'));

    function assertThumbnailView() {
      chrome.test.assertTrue(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertFalse(
          attachmentButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'true', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'false', attachmentButton.getAttribute('aria-selected'));

      chrome.test.assertFalse(thumbnailBar.hidden);
      chrome.test.assertTrue(attachmentBar.hidden);
    }

    function assertAttachmentView() {
      chrome.test.assertFalse(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertTrue(
          attachmentButton.parentElement!.classList.contains('selected'));

      chrome.test.assertEq(
          'false', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertEq(
          'true', attachmentButton.getAttribute('aria-selected'));

      chrome.test.assertTrue(thumbnailBar.hidden);
      chrome.test.assertFalse(attachmentBar.hidden);
    }

    // Sidebar starts on thumbnail view.
    assertThumbnailView();

    // Click on attachment view.
    attachmentButton.click();
    assertAttachmentView();

    // Return to thumbnail view.
    thumbnailButton.click();
    assertThumbnailView();

    // Arrow keys toggle through thumbnail and attachment view.

    // Thumbnail -> Attachment
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertAttachmentView();

    // Attachment -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertThumbnailView();

    // Thumbnail -> Attachment
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertAttachmentView();

    // Attachment -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertThumbnailView();

    chrome.test.succeed();
  },

  function testTabIconsHidden() {
    const sidenav = createSidenav();
    chrome.test.assertEq(0, sidenav.bookmarks.length);
    chrome.test.assertEq(0, sidenav.attachments.length);
    chrome.test.assertTrue(sidenav.getHideIconsForTesting());
    chrome.test.succeed();
  },

  function testTabIconsHiddenWithBookmarks() {
    const sidenav = createSidenav();

    // Add dummy bookmarks so that the buttons appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1, children: []},
      {title: 'Bar', page: 2, children: []},
    ];

    chrome.test.assertEq(2, sidenav.bookmarks.length);
    chrome.test.assertEq(0, sidenav.attachments.length);
    chrome.test.assertFalse(sidenav.getHideIconsForTesting());
    chrome.test.succeed();
  },

  function testTabIconsHiddenWithAttachments() {
    const sidenav = createSidenav();

    // Add dummy attachments so that the buttons appear.
    sidenav.attachments = [
      {name: 'attachment1', size: 10, readable: true},
      {name: 'attachment2', size: -1, readable: true},
    ];

    chrome.test.assertEq(0, sidenav.bookmarks.length);
    chrome.test.assertEq(2, sidenav.attachments.length);
    chrome.test.assertFalse(sidenav.getHideIconsForTesting());
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
