// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface PerformanceBrowserProxy {
  getDeviceHasBattery(): Promise<boolean>;
  openBatterySaverFeedbackDialog(): void;
  openHighEfficiencyFeedbackDialog(): void;
  validateTabDiscardExceptionRule(rule: string): Promise<boolean>;
}

export class PerformanceBrowserProxyImpl implements PerformanceBrowserProxy {
  getDeviceHasBattery() {
    return sendWithPromise('getDeviceHasBattery');
  }

  openBatterySaverFeedbackDialog() {
    chrome.send('openBatterySaverFeedbackDialog');
  }

  openHighEfficiencyFeedbackDialog() {
    chrome.send('openHighEfficiencyFeedbackDialog');
  }

  validateTabDiscardExceptionRule(rule: string) {
    return sendWithPromise('validateTabDiscardExceptionRule', rule);
  }

  static getInstance(): PerformanceBrowserProxy {
    return instance || (instance = new PerformanceBrowserProxyImpl());
  }

  static setInstance(obj: PerformanceBrowserProxy) {
    instance = obj;
  }
}

let instance: PerformanceBrowserProxy|null = null;
