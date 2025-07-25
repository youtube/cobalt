// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used to prevent multiple instances of the extension from running
 * simultaneously.
 */
export class InstanceChecker {
  static isActiveInstance() {
    // In 'split' manifest mode, the extension system runs two copies of the
    // extension. One in an incognito context; the other not. In guest mode, the
    // extension system runs only the extension in an incognito context. To
    // prevent doubling of this extension, only continue for one context.
    const manifest =
        /** @type {{incognito: (string|undefined)}} */ (
            chrome.runtime.getManifest());
    return manifest.incognito !== 'split' ||
        chrome.extension.inIncognitoContext;
  }

  static closeExtraInstances() {
    if (!InstanceChecker.isActiveInstance()) {
      window.close();
    }
  }
}
