// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/**
 * @constructor
 * @extends {HTMLElement}
 */
function CrViewManagerElement() {}

/**
 * @param {string} newViewId
 * @param {string=} enterAnimation
 * @param {string=} exitAnimation
 * @return {!Promise}
 */
CrViewManagerElement.prototype.switchView = function(
    newViewId, enterAnimation, exitAnimation) {};
