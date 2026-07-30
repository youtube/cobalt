// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TextBoxRect, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {convertRotatedCoordinates, pageToScreenCoordinates, screenToPageCoordinates} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {assertDeepEquals, MockDocumentDimensions, setUpInkTestContext} from './test_util.js';

let viewport: Viewport;

function setUpTestContext() {
  const context = setUpInkTestContext();
  viewport = context.viewport;
}

// Simulates the way the viewport is rotated from the plugin by setting updated
// DocumentDimensions. Assumes a non-rotated pageWidth of 400 and pageHeight of
// 500.
function rotateViewport(orientation: number) {
  const rotatedDocumentDimensions = new MockDocumentDimensions(0, 0);
  if (orientation === 0 || orientation === 2) {
    rotatedDocumentDimensions.addPage(400, 500);
  } else {
    rotatedDocumentDimensions.addPage(500, 400);
  }
  rotatedDocumentDimensions.layoutOptions = {
    defaultPageOrientation: orientation,
    direction: 2,  // LTR
    twoUpViewEnabled: false,
  };
  viewport.setDocumentDimensions(rotatedDocumentDimensions);
}

chrome.test.runTests([
  function testConvertRotatedCoordinates() {
    // Page size: 400x500
    // Rect at 0 rotation: X=50, Y=50, W=100, H=20
    const rect:
        TextBoxRect = {locationX: 50, locationY: 50, width: 100, height: 20};

    // Test 0 -> 1 (90 deg CW)
    // newPageWidth=500, newPageHeight=400.
    // X_new = pageHeightNR - Y - H = 500 - 50 - 20 = 430.
    // Y_new = X = 50.
    // W_new = H = 20.
    // H_new = W = 100.
    const rotated1 = convertRotatedCoordinates(rect, 0, 1, 500, 400);
    assertDeepEquals(
        {locationX: 430, locationY: 50, width: 20, height: 100}, rotated1);

    // Test 1 -> 0 (Undo 90 deg CW)
    const reverted1 = convertRotatedCoordinates(rotated1, 1, 0, 400, 500);
    assertDeepEquals(rect, reverted1);

    // Test 0 -> 2 (180 deg)
    // newPageWidth=400, newPageHeight=500.
    // X_new = pageWidthNR - X - W = 400 - 50 - 100 = 250.
    // Y_new = pageHeightNR - Y - H = 500 - 50 - 20 = 430.
    // W_new = W = 100.
    // H_new = H = 20.
    const rotated2 = convertRotatedCoordinates(rect, 0, 2, 400, 500);
    assertDeepEquals(
        {locationX: 250, locationY: 430, width: 100, height: 20}, rotated2);

    // Test 2 -> 0
    const reverted2 = convertRotatedCoordinates(rotated2, 2, 0, 400, 500);
    assertDeepEquals(rect, reverted2);

    chrome.test.succeed();
  },

  function testPageToScreenCoordinates() {
    setUpTestContext();
    const pageRect:
        TextBoxRect = {locationX: 50, locationY: 50, width: 100, height: 20};

    // 1. Zoom 1.0, Rotation 0
    let screenRect = pageToScreenCoordinates(0, pageRect, viewport);
    assertDeepEquals(
        {locationX: 105, locationY: 53, width: 100, height: 20}, screenRect);

    // 2. Zoom 2.0, Rotation 0
    viewport.setZoom(2.0);
    screenRect = pageToScreenCoordinates(0, pageRect, viewport);
    assertDeepEquals(
        {locationX: 110, locationY: 106, width: 200, height: 40}, screenRect);

    // 3. Zoom 1.0, Rotation 90 deg CW
    viewport.setZoom(1.0);
    rotateViewport(1);  // 90 deg CW
    screenRect = pageToScreenCoordinates(0, pageRect, viewport);
    assertDeepEquals(
        {locationX: 425, locationY: 53, width: 20, height: 100}, screenRect);

    chrome.test.succeed();
  },

  function testScreenToPageCoordinates() {
    setUpTestContext();
    const pageRect:
        TextBoxRect = {locationX: 50, locationY: 50, width: 100, height: 20};

    // 1. Zoom 1.0, Rotation 0
    const screenRect1 = {locationX: 105, locationY: 53, width: 100, height: 20};
    const calculatedPageRect1 =
        screenToPageCoordinates(0, screenRect1, viewport);
    assertDeepEquals(pageRect, calculatedPageRect1);

    // 2. Zoom 2.0, Rotation 0
    viewport.setZoom(2.0);
    const screenRect2 =
        {locationX: 110, locationY: 106, width: 200, height: 40};
    const calculatedPageRect2 =
        screenToPageCoordinates(0, screenRect2, viewport);
    assertDeepEquals(pageRect, calculatedPageRect2);

    // 3. Zoom 1.0, Rotation 90 deg CW
    viewport.setZoom(1.0);
    rotateViewport(1);
    const screenRect3 = {locationX: 425, locationY: 53, width: 20, height: 100};
    const calculatedPageRect3 =
        screenToPageCoordinates(0, screenRect3, viewport);
    assertDeepEquals(pageRect, calculatedPageRect3);

    chrome.test.succeed();
  },
]);
