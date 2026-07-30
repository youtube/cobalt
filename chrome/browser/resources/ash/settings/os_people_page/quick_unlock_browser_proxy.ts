// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * A helper object used by the "Lock screen" subpage to query the status of
 * the password and PIN authentication factors for quick unlock.
 */

export interface QuickUnlockBrowserProxy {
  /**
   * Fetches whether the password and PIN authentication factors are currently
   * configured for the user.
   */
  requestActiveAuthFactors(): Promise<{password: boolean, pin: boolean}>;
}

export class QuickUnlockBrowserProxyImpl implements QuickUnlockBrowserProxy {
  requestActiveAuthFactors(): Promise<{password: boolean, pin: boolean}> {
    return sendWithPromise('RequestActiveAuthFactors');
  }

  static getInstance(): QuickUnlockBrowserProxy {
    return instance || (instance = new QuickUnlockBrowserProxyImpl());
  }

  static setInstance(obj: QuickUnlockBrowserProxy): void {
    instance = obj;
  }
}

let instance: QuickUnlockBrowserProxy|null = null;
