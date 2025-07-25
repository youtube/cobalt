// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Finds the url of the image or link under the selected point. Sends the
 * found element (or an empty object if no links or images are found) back to
 * the application by posting a 'FindElementResultHandler' message.
 * The object returned in the message is of the same form as
 * `findElementAtPointInPageCoordinates` result.
 * @param requestId An identifier which be returned in the result
 *                 dictionary of this request.
 * @param x - horizontal center of the selected point in web view
 *                 coordinates.
 * @param y - vertical center of the selected point in web view
 *                 coordinates.
 */
function findElementAtPoint(
    requestId: string, x: number, y: number, webViewWidth: number,
    _webViewHeight: number) {
  const scale = getPageWidth() / webViewWidth;
  gCrWeb.contextMenuAllFrames.findElementAtPointInPageCoordinates(
      requestId, x * scale, y * scale);
}

/**
 * Returns maximum width of the web page.
 */
function getPageWidth(): number {
  const documentElement = document.documentElement;
  const documentBody = document.body;
  return Math.max(
      documentElement.clientWidth, documentElement.scrollWidth,
      documentElement.offsetWidth, documentBody.scrollWidth,
      documentBody.offsetWidth);
}

gCrWeb.contextMenu = {findElementAtPoint};
