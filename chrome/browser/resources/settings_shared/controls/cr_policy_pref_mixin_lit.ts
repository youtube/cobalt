// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs in Lit.
 */

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export const CrPolicyPrefMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<CrPolicyPrefMixinLitInterface> => {
  class CrPolicyPrefMixinLit extends superClass implements
      CrPolicyPrefMixinLitInterface {
    static get properties() {
      return {
        noExtensionIndicator: {type: Boolean},
        pref: {type: Object},
      };
    }

    accessor noExtensionIndicator: boolean = false;
    accessor pref: chrome.settingsPrivate.PrefObject|undefined = undefined;

    /**
     * Is the |pref| controlled by something that prevents user control of
     * the preference.
     * @return True if |this.pref| is controlled by an enforced policy.
     */
    isPrefEnforced(): boolean {
      return !!this.pref &&
          this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
    }

    /**
     * @return True if |this.pref| has a recommended or enforced policy.
     */
    hasPrefPolicyIndicator(): boolean {
      if (!this.pref) {
        return false;
      }
      if (this.noExtensionIndicator &&
          this.pref.controlledBy ===
              chrome.settingsPrivate.ControlledBy.EXTENSION) {
        return false;
      }
      return this.isPrefEnforced() ||
          this.pref.enforcement ===
          chrome.settingsPrivate.Enforcement.RECOMMENDED;
    }
  }

  return CrPolicyPrefMixinLit;
};

export interface CrPolicyPrefMixinLitInterface {
  noExtensionIndicator: boolean;
  pref: chrome.settingsPrivate.PrefObject|undefined;
  isPrefEnforced(): boolean;
  hasPrefPolicyIndicator(): boolean;
}
