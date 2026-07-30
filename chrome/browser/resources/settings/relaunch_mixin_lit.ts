// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {LifetimeBrowserProxy} from '/shared/settings/lifetime_browser_proxy.js';
import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';

export enum RestartType {
  RESTART,
  RELAUNCH,
}

type Constructor<T> = new (...args: any[]) => T;

export const RelaunchMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<RelaunchMixinLitInterface> => {
  class RelaunchMixinLit extends superClass implements
      RelaunchMixinLitInterface {
    static get properties() {
      return {
        shouldShowRelaunchDialog: {type: Boolean},
      };
    }

    accessor shouldShowRelaunchDialog: boolean = false;
    private lifetimeBrowserProxy_: LifetimeBrowserProxy;

    constructor(...args: any[]) {
      super(...args);
      this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
    }

    onRelaunchDialogClose(_event: Event) {
      this.shouldShowRelaunchDialog = false;
    }

    private performRestartInternal_(restartType: RestartType) {
      if (RestartType.RESTART === restartType) {
        this.lifetimeBrowserProxy_.restart();
      } else if (RestartType.RELAUNCH === restartType) {
        this.lifetimeBrowserProxy_.relaunch();
      } else {
        assertNotReached();
      }
    }

    // <if expr="not is_chromeos">
    private async performRestartForNonChromeOs_(
        restartType: RestartType, alwaysShowDialog: boolean) {
      const shouldShowDialog =
          await this.lifetimeBrowserProxy_.shouldShowRelaunchConfirmationDialog(
              alwaysShowDialog);
      if (!shouldShowDialog) {
        this.performRestartInternal_(restartType);
        return;
      }

      this.shouldShowRelaunchDialog = true;
    }
    // </if>

    performRestart(restartType: RestartType, alwaysShowDialog?: boolean) {
      if (alwaysShowDialog == null) {
        alwaysShowDialog = false;
      }

      // <if expr="is_chromeos">
      this.performRestartInternal_(restartType);
      // </if>

      // <if expr="not is_chromeos">
      this.performRestartForNonChromeOs_(restartType, alwaysShowDialog);
      // </if>
    }
  }
  return RelaunchMixinLit;
};

export interface RelaunchMixinLitInterface {
  shouldShowRelaunchDialog: boolean;
  onRelaunchDialogClose(event: Event): void;
  performRestart(restartType: RestartType, alwaysShowDialog?: boolean): void;
}
