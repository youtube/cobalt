// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InkTextAnnotationsElement, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {Ink2Manager} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getTestAnnotation, MockDocumentDimensions, setUpInkTestContext} from './test_util.js';

let viewport: Viewport;

function setUpInk2Manager(): Ink2Manager {
  Ink2Manager.setInstance(null);
  const context = setUpInkTestContext();
  viewport = context.viewport;
  return Ink2Manager.getInstance();
}

function createAnnotationsElement(): InkTextAnnotationsElement {
  document.body.innerHTML = '';
  const element = document.createElement('ink-text-annotations');
  element.viewport = viewport;
  document.body.appendChild(element);
  return element;
}

function getPlaceholders(element: InkTextAnnotationsElement):
    NodeListOf<HTMLElement> {
  return element.shadowRoot.querySelectorAll('.placeholder');
}

chrome.test.runTests([
  async function testPlaceholdersRenderedAndSorted() {
    const manager = setUpInk2Manager();

    // Add a second page to the document dimensions to support pageIndex 1.
    const dimensions = new MockDocumentDimensions(0, 0);
    dimensions.addPage(400, 500);  // Page 0: 400x500
    dimensions.addPage(400, 500);  // Page 1: 400x500
    viewport.setDocumentDimensions(dimensions);

    // Commit annotations with SCREEN coordinates. Ink2Manager will convert them
    // to PAGE coordinates.
    // Page 0 screen offset: X=55, Y=3. Page 1 screen offset: X=55, Y=503
    // (at 1.0 zoom).

    // Annotation 1: Page 0, Screen X=105, Y=103 (Page X=50, Y=100)
    const annotation1 = {
      ...getTestAnnotation(1),
      text: 'Annotation 1',
      textBoxRect: {height: 20, locationX: 105, locationY: 103, width: 100},
    };

    // Annotation 2: Page 0, Screen X=105, Y=53 (Page X=50, Y=50)
    const annotation2 = {
      ...getTestAnnotation(2),
      text: 'Annotation 2',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };

    // Annotation 3: Page 1, Screen X=65, Y=513 (Page X=10, Y=10)
    const annotation3 = {
      ...getTestAnnotation(3),
      pageIndex: 1,
      text: 'Annotation 3',
      textBoxRect: {height: 20, locationX: 65, locationY: 513, width: 100},
    };

    // Commit them to manager (out of order)
    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation3, true, []);
    manager.commitTextAnnotation(annotation2, true, []);

    const element = createAnnotationsElement();
    await microtasksFinished();

    const placeholders = getPlaceholders(element);
    chrome.test.assertEq(3, placeholders.length);

    // Verify A11y attributes on inner container
    const container = element.shadowRoot.querySelector('#container');
    assert(container);
    chrome.test.assertEq('list', container.getAttribute('role'));
    chrome.test.assertEq(
        'Text Annotations', container.getAttribute('aria-label'));

    // Verify A11y attributes on placeholders
    chrome.test.assertEq('listitem', placeholders[0]!.getAttribute('role'));
    chrome.test.assertEq('0', placeholders[0]!.getAttribute('tabindex'));

    // Verify sorting: annotation2 (Page 0, Y=50) -> annotation1 (Page 0, Y=100)
    // -> annotation3 (Page 1)
    chrome.test.assertEq(
        'Annotation 2', placeholders[0]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 1', placeholders[1]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 3', placeholders[2]!.getAttribute('aria-label'));

    // Assert computed style which includes the CSS offsets (matching
    // ink-text-box): Left: 105 - 12 = 93px Top: 53 - 10 = 43px Width: 100 + 24
    // = 124px Height: 20 + 20 = 40px
    const style2 = window.getComputedStyle(placeholders[0]!);
    chrome.test.assertEq('93px', style2.left);
    chrome.test.assertEq('43px', style2.top);
    chrome.test.assertEq('124px', style2.width);
    chrome.test.assertEq('40px', style2.height);

    chrome.test.succeed();
  },
]);
