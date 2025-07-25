// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {AnchorType, positionPopupAroundElement, positionPopupAtPoint} from './position_util.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
// clang-format on

/** @type {!HTMLElement} */
let anchor;
/** @type {!HTMLElement} */
let popup;
// @ts-ignore: error TS7034: Variable 'anchorParent' implicitly has type 'any'
// in some locations where its type cannot be determined.
let anchorParent;
// @ts-ignore: error TS7034: Variable 'oldGetBoundingClientRect' implicitly has
// type 'any' in some locations where its type cannot be determined.
let oldGetBoundingClientRect;
// @ts-ignore: error TS7034: Variable 'availRect' implicitly has type 'any' in
// some locations where its type cannot be determined.
let availRect;

/**
 * @param {number} w width
 * @param {number} h height
 * @constructor
 */
function MockRect(w, h) {
  /** @type {number} */
  this.left = 0;

  /** @type {number} */
  this.top = 0;

  /** @type {number} */
  this.width = w;

  /** @type {number} */
  this.height = h;

  /** @type {number} */
  this.right = this.left + w;

  /** @type {number} */
  this.bottom = this.top + h;
}

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <style>
      html, body {
        margin: 0;
        width: 100%;
        height: 100%;
      }

      #anchor {
        position: absolute;
        width: 10px;
        height: 10px;
        background: green;
      }

      #popup {
        position: absolute;
        top: 0;
        left: 0;
        width: 100px;
        height: 100px;
        background: red;
      }
    </style>

    <div id="anchor"></div>
    <div id="popup"></div>
  `;

  anchor = /** @type {!HTMLElement} */ (document.getElementById('anchor'));
  popup = /** @type {!HTMLElement} */ (document.getElementById('popup'));
  anchorParent = anchor.offsetParent;
  // @ts-ignore: error TS18047: 'anchorParent' is possibly 'null'.
  oldGetBoundingClientRect = anchorParent.getBoundingClientRect;

  anchor.style.top = '100px';
  anchor.style.left = '100px';
  availRect = new MockRect(200, 200);
  // @ts-ignore: error TS18047: 'anchorParent' is possibly 'null'.
  anchorParent.getBoundingClientRect = function() {
    // @ts-ignore: error TS7005: Variable 'availRect' implicitly has an 'any'
    // type.
    return availRect;
  };
}

export function tearDown() {
  document.documentElement.dir = 'ltr';
  // @ts-ignore: error TS7005: Variable 'oldGetBoundingClientRect' implicitly
  // has an 'any' type.
  anchorParent.getBoundingClientRect = oldGetBoundingClientRect;
}

export function testAbovePrimary() {
  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);

  assertEquals('auto', popup.style.top);
  assertEquals('100px', popup.style.bottom);

  anchor.style.top = '90px';
  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);
  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);
}

export function testBelowPrimary() {
  // ensure enough below
  anchor.style.top = '90px';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  // ensure not enough below
  anchor.style.top = '100px';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);
  assertEquals('auto', popup.style.top);
  assertEquals('100px', popup.style.bottom);
}

export function testBeforePrimary() {
  positionPopupAroundElement(anchor, popup, AnchorType.BEFORE);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  anchor.style.left = '90px';
  positionPopupAroundElement(anchor, popup, AnchorType.BEFORE);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testBeforePrimaryRtl() {
  document.documentElement.dir = 'rtl';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  anchor.style.left = '90px';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testAfterPrimary() {
  // ensure enough to the right
  anchor.style.left = '90px';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  // ensure not enough below
  anchor.style.left = '100px';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);
  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);
}

export function testAfterPrimaryRtl() {
  document.documentElement.dir = 'rtl';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  // ensure not enough below
  anchor.style.left = '90px';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testAboveSecondary() {
  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  anchor.style.left = '110px';

  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);

  assertEquals('auto', popup.style.left);
  assertEquals('80px', popup.style.right);
}

export function testAboveSecondaryRtl() {
  document.documentElement.dir = 'rtl';

  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testAboveSecondarySwappedAlign() {
  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE, true);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  positionPopupAroundElement(anchor, popup, AnchorType.ABOVE, true);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testBelowSecondary() {
  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  anchor.style.left = '110px';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);

  assertEquals('auto', popup.style.left);
  assertEquals('80px', popup.style.right);
}

export function testBelowSecondaryRtl() {
  document.documentElement.dir = 'rtl';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testBelowSecondarySwappedAlign() {
  positionPopupAroundElement(anchor, popup, AnchorType.BELOW, true);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  positionPopupAroundElement(anchor, popup, AnchorType.BELOW, true);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

export function testBeforeSecondary() {
  positionPopupAroundElement(anchor, popup, AnchorType.BEFORE);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  anchor.style.top = '110px';

  positionPopupAroundElement(anchor, popup, AnchorType.BEFORE);

  assertEquals('auto', popup.style.top);
  assertEquals('80px', popup.style.bottom);
}

export function testAfterSecondary() {
  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  anchor.style.top = '110px';

  positionPopupAroundElement(anchor, popup, AnchorType.AFTER);

  assertEquals('auto', popup.style.top);
  assertEquals('80px', popup.style.bottom);
}

export function testPositionAtPoint() {
  positionPopupAtPoint(100, 100, popup);

  assertEquals('100px', popup.style.left);
  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.right);
  assertEquals('auto', popup.style.bottom);

  positionPopupAtPoint(100, 150, popup);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.top);
  assertEquals('auto', popup.style.right);
  assertEquals('50px', popup.style.bottom);

  positionPopupAtPoint(150, 150, popup);

  assertEquals('auto', popup.style.left);
  assertEquals('auto', popup.style.top);
  assertEquals('50px', popup.style.right);
  assertEquals('50px', popup.style.bottom);
}
