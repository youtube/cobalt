// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ResetBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestResetBrowserProxy extends TestBrowserProxy implements
    ResetBrowserProxy {
  private tamperedPreferencePaths_: string[] = [];
  private performResetProfileSettingsPromise_: Promise<void>|null = null;
  private resolveResetProfileSettings_: (() => void)|null = null;

  constructor() {
    super([
      'performResetProfileSettings',
      'onHideResetProfileDialog',
      'onHideResetProfileBanner',
      'onShowResetProfileDialog',
      'showReportedSettings',
      'getTriggeredResetToolName',
      'getTamperedPreferencePaths',
    ]);
  }

  getTamperedPreferencePaths() {
    this.methodCalled('getTamperedPreferencePaths');
    return Promise.resolve(this.tamperedPreferencePaths_);
  }

  setTamperedPreferencePaths(paths: string[]) {
    this.tamperedPreferencePaths_ = paths;
  }

  performResetProfileSettings(_sendSettings: boolean, requestOrigin: string) {
    this.methodCalled('performResetProfileSettings', requestOrigin);
    if (this.performResetProfileSettingsPromise_) {
      return this.performResetProfileSettingsPromise_;
    }
    return Promise.resolve();
  }

  setPerformResetProfileSettingsPromise() {
    this.performResetProfileSettingsPromise_ = new Promise(resolve => {
      this.resolveResetProfileSettings_ = resolve;
    });
  }

  resolvePerformResetProfileSettings() {
    if (this.resolveResetProfileSettings_) {
      this.resolveResetProfileSettings_();
      this.performResetProfileSettingsPromise_ = null;
      this.resolveResetProfileSettings_ = null;
    }
  }

  onHideResetProfileDialog() {
    this.methodCalled('onHideResetProfileDialog');
  }

  onHideResetProfileBanner() {
    this.methodCalled('onHideResetProfileBanner');
  }

  onShowResetProfileDialog() {
    this.methodCalled('onShowResetProfileDialog');
  }

  showReportedSettings() {
    this.methodCalled('showReportedSettings');
  }

  getTriggeredResetToolName() {
    this.methodCalled('getTriggeredResetToolName');
    return Promise.resolve('WonderfulAV');
  }
}
