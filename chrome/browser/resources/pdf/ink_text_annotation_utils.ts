// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

import type {TextBoxRect} from './constants.js';
import type {Viewport} from './viewport.js';

/**
 * Converts `rect` from `oldRotations` clockwise rotations to `newRotations`
 * clockwise rotations. `newPageWidth` should be the page width in
 * `newRotations` coordinates, and `newPageHeight` should be the page height in
 * `newRotations` coordinates.
 */
export function convertRotatedCoordinates(
    rect: TextBoxRect, oldRotations: number, newRotations: number,
    newPageWidth: number, newPageHeight: number): TextBoxRect {
  const pageWidthNR = newRotations % 2 === 0 ? newPageWidth : newPageHeight;
  const pageHeightNR = newRotations % 2 === 0 ? newPageHeight : newPageWidth;
  const nonRotated: TextBoxRect = {
    locationX: rect.locationX,
    locationY: rect.locationY,
    width: oldRotations % 2 === 0 ? rect.width : rect.height,
    height: oldRotations % 2 === 0 ? rect.height : rect.width,
  };
  switch (oldRotations % 4) {
    case 0:
      // Already populated correctly.
      break;
    case 1:
      nonRotated.locationX = rect.locationY;
      nonRotated.locationY = pageHeightNR - rect.locationX - rect.width;
      break;
    case 2:
      nonRotated.locationX = pageWidthNR - rect.locationX - rect.width;
      nonRotated.locationY = pageHeightNR - rect.locationY - rect.height;
      break;
    case 3:
      nonRotated.locationX = pageWidthNR - rect.locationY - rect.height;
      nonRotated.locationY = rect.locationX;
      break;
    default:
      assertNotReached();
  }

  const newRotated = {
    locationX: nonRotated.locationX,
    locationY: nonRotated.locationY,
    width: newRotations % 2 === 0 ? nonRotated.width : nonRotated.height,
    height: newRotations % 2 === 0 ? nonRotated.height : nonRotated.width,
  };
  switch (newRotations % 4) {
    case 0:
      break;
    case 1:
      newRotated.locationX =
          pageHeightNR - nonRotated.locationY - nonRotated.height;
      newRotated.locationY = nonRotated.locationX;
      break;
    case 2:
      newRotated.locationX =
          pageWidthNR - nonRotated.locationX - nonRotated.width;
      newRotated.locationY =
          pageHeightNR - nonRotated.locationY - nonRotated.height;
      break;
    case 3:
      newRotated.locationX = nonRotated.locationY;
      newRotated.locationY =
          pageWidthNR - nonRotated.locationX - nonRotated.width;
      break;
    default:
      assertNotReached();
  }
  return newRotated;
}

/**
 * Converts `pageRect` on page `pageIndex` to screen coordinates.
 */
export function pageToScreenCoordinates(
    pageIndex: number, pageRect: TextBoxRect, viewport: Viewport): TextBoxRect {
  const pageDimensions = viewport.getPageScreenRect(pageIndex);
  const zoom = viewport.getZoom();

  // Apply zoom.
  const zoomed = {
    locationX: pageRect.locationX * zoom,
    locationY: pageRect.locationY * zoom,
    width: pageRect.width * zoom,
    height: pageRect.height * zoom,
  };

  // Apply rotation
  const rotated = convertRotatedCoordinates(
      zoomed, 0, viewport.getClockwiseRotations(), pageDimensions.width,
      pageDimensions.height);

  // Apply offsets.
  return {
    locationX: rotated.locationX + pageDimensions.x,
    locationY: rotated.locationY + pageDimensions.y,
    height: rotated.height,
    width: rotated.width,
  };
}

/**
 * Converts `screenRect` on page `pageIndex` to page coordinates.
 */
export function screenToPageCoordinates(
    pageIndex: number, screenRect: TextBoxRect,
    viewport: Viewport): TextBoxRect {
  const zoom = viewport.getZoom();
  const pageDimensions = viewport.getPageScreenRect(pageIndex);

  // Undo offset
  const noOffset = {
    locationX: screenRect.locationX - pageDimensions.x,
    locationY: screenRect.locationY - pageDimensions.y,
    width: screenRect.width,
    height: screenRect.height,
  };

  // Undo rotation
  const rotations = viewport.getClockwiseRotations();
  const pageWidth =
      rotations % 2 === 0 ? pageDimensions.width : pageDimensions.height;
  const pageHeight =
      rotations % 2 === 0 ? pageDimensions.height : pageDimensions.width;
  const noRotation =
      convertRotatedCoordinates(noOffset, rotations, 0, pageWidth, pageHeight);

  // Undo zoom.
  return {
    height: noRotation.height / zoom,
    locationX: noRotation.locationX / zoom,
    locationY: noRotation.locationY / zoom,
    width: noRotation.width / zoom,
  };
}
