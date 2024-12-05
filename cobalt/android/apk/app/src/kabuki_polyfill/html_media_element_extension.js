/**
 * @license
 * Copyright The Cobalt Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

export function initializeHTMLMediaElement() {
    if (typeof HTMLMediaElementExtension !== 'undefined') {
        HTMLMediaElement.prototype.canPlayType = HTMLMediaElementExtension.canPlayType;
    }
}
