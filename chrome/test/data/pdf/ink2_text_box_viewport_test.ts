// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DocumentDimensions} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, getTestAnnotation, initializeBox, setupTextBoxTest} from './ink2_text_box_test_utils.js';
import {MockDocumentDimensions} from './test_util.js';

chrome.test.runTests([
  async function testViewportChanges() {
    // Use 300x300 viewport so that the 800x1000 page is larger and scrollable.
    // zeroScrollbars is set to true here to simplify layout math.
    const {textbox, viewport} =
        await setupTextBoxTest(300, 300, 800, 1000, /*zeroScrollbars=*/ true);

    initializeBox(100, 100, 410, 303);
    await microtasksFinished();
    textbox.viewportChanged();  // Sync with real viewport initially.
    await microtasksFinished();

    // Baseline coordinates:
    // Page (800) > viewport (300), so no centering - page offset is (5, 3)
    // for the shadows.
    // Left = 400 + 5 - 17 = 388, Top = 300 + 3 - 15 = 288.
    // Width = 100 * 1.0 + 24 (padding), height = 100 * 1.0 + 20 (padding)
    assertPositionAndSize(textbox, '124px', '120px', '388px', '288px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    viewport.setZoom(0.5);
    await microtasksFinished();
    // Zoom 0.5: Page (400) > viewport (300), so no centering.
    // Page offset is (2.5, 1.5) for shadows.
    // Page-relative X (400) is scaled: 400 * 0.5 = 200.
    // Left = 200 + 2.5 - 17 = 185.5, Top = 300 * 0.5 + 1.5 - 15 = 136.5
    assertPositionAndSize(textbox, '74px', '70px', '185.5px', '136.5px');
    chrome.test.assertEq(
        '6px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 2.0.
    viewport.setZoom(2.0);
    await microtasksFinished();
    // Zoom 2.0: Page (1600) > viewport (300), so no centering.
    // Page offsets (5 * 2, 3 * 2) for shadows = (10, 6).
    // Page-relative X (400) is scaled: 400 * 2 = 800.
    // Left = 800 + 10 - 17 = 793, Top = 300 * 2 + 6 - 15 = 591
    assertPositionAndSize(textbox, '224px', '220px', '793px', '591px');
    chrome.test.assertEq(
        '24px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a scroll + resetting zoom to 1.0.
    viewport.setZoom(1.0);
    viewport.scrollTo({x: 100, y: 100});
    await microtasksFinished();
    // Scroll 100, 100: Coordinates shift by exactly -100px!
    // left = 388 - 100 = 288px. top = 288 - 100 = 188px.
    assertPositionAndSize(textbox, '124px', '120px', '288px', '188px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where start of page is no longer in the viewport and the textbox
    // ends up off screen.
    // Scroll X to its maximum of 500px (800 page - 300 viewport), and Y to
    // 503px.
    viewport.scrollTo({x: 500, y: 503});
    await microtasksFinished();
    // left = 388 - 500 = -112px. top = 288 - 503 = -215px.
    assertPositionAndSize(textbox, '124px', '120px', '-112px', '-215px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    chrome.test.succeed();
  },

  async function testViewportRotationChanges() {
    // Set viewport to 100x100, and initial page to landscape 100x80 (starts at
    // Rot 3). Pass zeroScrollbars = true. This aligns the real Viewport page
    // screen offsets exactly with the original test's mock offsets (x=15 for
    // portrait, x=5 for landscape).
    const {textbox, viewport} =
        await setupTextBoxTest(100, 100, 100, 80, /*zeroScrollbars=*/ true);

    function initializeBoxWithOrientation(
        width: number, height: number, x: number, y: number,
        orientation: number) {
      const pageDimensions = viewport.getPageScreenRect(0);
      const annotation = getTestAnnotation({
        height,
        width,
        locationX: x,
        locationY: y,
      });
      annotation.text = '';
      annotation.textOrientation = orientation;

      textbox.annotation = annotation;
      textbox.pageDimensions = pageDimensions;
    }

    function initAndSyncBox(
        width: number, height: number, x: number, y: number,
        orientation: number): Promise<void> {
      initializeBoxWithOrientation(width, height, x, y, orientation);
      return microtasksFinished();
    }

    function updateViewportWithClockwiseRotations(rotations: number):
        Promise<void> {
      const rotatedDocumentDimensions = new MockDocumentDimensions(80, 100);
      if (rotations === 0 || rotations === 2) {
        rotatedDocumentDimensions.addPage(80, 100);
      } else {
        rotatedDocumentDimensions.addPage(100, 80);
      }
      rotatedDocumentDimensions.layoutOptions = {
        defaultPageOrientation: rotations,
        direction: 2,  // LTR
        twoUpViewEnabled: false,
      };
      viewport.setDocumentDimensions(
          rotatedDocumentDimensions as unknown as DocumentDimensions);
      return microtasksFinished();
    }

    function assertTextboxStyles(expectedTextRotation: number) {
      const expectedTransform =
          expectedTextRotation === 2 ? 'matrix(-1, 0, 0, -1, 0, 0)' : 'none';
      let expectedWritingMode = 'horizontal-tb';
      if (expectedTextRotation === 1) {
        expectedWritingMode = 'vertical-rl';
      } else if (expectedTextRotation === 3) {
        expectedWritingMode = 'sideways-lr';
      }
      const styles = getComputedStyle(textbox.$.textbox);
      chrome.test.assertEq(
          expectedTransform, styles.getPropertyValue('transform'));
      chrome.test.assertEq(
          expectedWritingMode, styles.getPropertyValue('writing-mode'));
    }

    // Initialize to a 50x48 box at 20, 30 + page offsets. Make box rotated
    // by 90 degrees clockwise compared to the PDF. This happens when the
    // viewport is rotated by 90 degrees CCW and the user creates a new
    // annotation, so simulate that scenario here.
    await updateViewportWithClockwiseRotations(3);
    // Initialize with screen coordinates 25, 33 which translate to page
    // coordinates of 20, 30 after adjusting for the shadows.
    await initAndSyncBox(50, 48, 25, 33, 1);

    // Left: 20 (page coordinate) + 5 (page offset) - 17 (css padding) = 8
    // Top: 30 (page coordinate) + 3 (page offset) - 15 (css padding) = 18
    // Width = 50 + 24px padding = 74, Height = 48 + 20px padding = 68
    assertPositionAndSize(textbox, '74px', '68px', '8px', '18px');
    // Textbox is non-rotated relative to the current viewport orientation.
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(0);
    // This converts the textbox to the non-rotated coordinates.
    // Left: [pageWidthNR (70) - originalY (30) - originalHeight (48)] = -8
    // Left adjusted = left + pageX (15) - 15 (CSS padding, swapped) = -8
    // Top: originalX (20) = 20
    // Top adjusted = top + pageY (3) - 17 (CSS padding, swapped) = 6
    assertPositionAndSize(textbox, '68px', '74px', '-8px', '6px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(1);
    // Left: pageHeightNR (90) - nonRotatedY (20) - nonRotatedHeight (50) = 20
    // Adjusted left = 20 + pageX (5) - 17 (CSS padding) = 8
    // Top: nonRotatedX = -8
    // Top adjusted = -8 + pageY (3) - 15 (CSS padding) = -20
    assertPositionAndSize(textbox, '74px', '68px', '8px', '-20px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(2);
    // Left: pageWidthNR (70) - nonRotatedX (-8) - nonRotatedWidth (48) = 30
    // Left adjusted = 30 + pageX (15) - 15 (CSS padding, swapped) = 30
    // Top: pageHeightNR (90) - nonRotatedY (20) - nonRotatedHeight (50) = 20
    // Top adjusted = 20 + pageY (3) - 17 (CSS padding, swapped) = 6
    assertPositionAndSize(textbox, '68px', '74px', '30px', '6px');
    assertTextboxStyles(3);

    // Loop Closure: Verify full position and styles return exactly to the
    // initial state.
    await updateViewportWithClockwiseRotations(3);
    assertPositionAndSize(textbox, '74px', '68px', '8px', '18px');
    assertTextboxStyles(0);

    // Now initialize a box with no rotation relative to the PDF, at the same
    // location. This happens when the viewport has no rotation when the box is
    // created.
    await updateViewportWithClockwiseRotations(0);
    // Initialize with screen coordinates 35, 33, page coordinates 20, 30
    await initAndSyncBox(50, 48, 35, 33, 0);

    // Left: originalX (20) + pageX (15) - 17 (CSS padding) = 18px
    // Top: originalY (30) + pageY (3) - 15 (CSS padding) = 18px
    assertPositionAndSize(textbox, '74px', '68px', '18px', '18px');
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(1);
    // Left: pageHeightNR (90) - originalY (30) - originalHeight (48) = 12
    // Left adjusted: 12 + pageX (5) - 15 (CSS padding, swapped) = 2
    // Top: originalX (20)
    // Top adjusted: 20 + pageY (3) - 17 (CSS padding, swapped) = 6
    assertPositionAndSize(textbox, '68px', '74px', '2px', '6px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(2);
    // Left: pageWidthNR (70) - originalX (20) - originalWidth (50) = 0
    // Left adjusted: 0 + pageX (15) - 17 (CSS padding) = -2
    // Top: pageHeightNR (90) - originalY (30) - originalHeight (48) = 12
    // Top adjusted = 12 + pageY (3) - 15 (CSS padding) = 0
    assertPositionAndSize(textbox, '74px', '68px', '-2px', '0px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(3);
    // Left: originalY (30)
    // Left adjusted: 30 + pageX (5) - 15 (CSS padding, swapped) = 20
    // Top: pageWidthNR (70) - originalX (20) - originalWidth (50) = 0
    // Top adjusted: 0 + pageY (3) - 17 (CSS padding, swapped) = -14
    assertPositionAndSize(textbox, '68px', '74px', '20px', '-14px');
    assertTextboxStyles(3);

    // Loop Closure: Verify full position and styles return exactly.
    await updateViewportWithClockwiseRotations(0);
    assertPositionAndSize(textbox, '74px', '68px', '18px', '18px');
    assertTextboxStyles(0);

    chrome.test.succeed();
  },

  async function testMoveViewportOnFocus() {
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    // Ensure the viewport is scrollable by zooming in. Also ensure it is
    // located top/left, where it is expected.
    viewport.setZoom(2.0);
    viewport.goToPageAndXy(0, 0, 0);

    // Initialize the top left corner to 60, 60 with default textbox width (222)
    // and height at 2x zoom (34).
    const annotation = getTestAnnotation(
        {height: 34, locationX: 60, locationY: 60, width: 222}, 2.0);
    annotation.text = '';
    textbox.annotation = annotation;
    textbox.pageDimensions = viewport.getPageScreenRect(0);
    await eventToPromise('textbox-focused-for-test', textbox);
    await microtasksFinished();
    const styles = getComputedStyle(textbox);
    chrome.test.assertEq('43px', styles.getPropertyValue('left'));
    chrome.test.assertEq('45px', styles.getPropertyValue('top'));

    // Scroll away from the textbox. Note this method accepts page coordinates.
    // Scrolling by 35 in page coordinates scrolls by 70 in screen coordinates
    // at 2x zoom. Blurring the textbox in case it is still holding focus, to
    // simulate how scroll would work if the user scrolled by clicking on the
    // scrollbars, or by moving focus to the plugin and scrolling with the
    // keyboard. This also ensures the textbox gets a focus event when focused
    // later.
    textbox.blur();
    viewport.goToPageAndXy(0, 35, 35);
    await microtasksFinished();
    chrome.test.assertEq('-27px', styles.getPropertyValue('left'));
    chrome.test.assertEq('-25px', styles.getPropertyValue('top'));

    // Focus the textbox, which should cause it to dispatch 'textbox-focused'.
    const focusEventPromise = eventToPromise('textbox-focused', textbox);
    mockPlugin.clearMessages();
    // Manually fire the focus event. Browser focus is not guaranteed in tests.
    textbox.focus();
    textbox.dispatchEvent(new FocusEvent('focus'));

    const focusEvent = (await focusEventPromise) as CustomEvent;
    chrome.test.assertTrue(!!focusEvent);
    // The box is at 60, 60 in viewport coordinates, and the viewport was
    // scrolled by 70, 70 (35 page pixels at 2x zoom). So the new
    // viewport-relative coordinates should be 60 - 70 = -10.
    chrome.test.assertEq(-10, focusEvent.detail.locationX);
    chrome.test.assertEq(-10, focusEvent.detail.locationY);
    chrome.test.assertEq(222, focusEvent.detail.width);
    chrome.test.assertEq(34, focusEvent.detail.height);

    chrome.test.succeed();
  },
]);
