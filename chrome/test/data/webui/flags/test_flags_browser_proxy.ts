// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExperimentalFeaturesData, FlagsBrowserProxy} from 'chrome://flags/flags_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFlagsBrowserProxy extends TestBrowserProxy implements
    FlagsBrowserProxy {
  private featureData: ExperimentalFeaturesData = {
    // Default feature data
    'supportedFeatures': [],
    'unsupportedFeatures': [],
    'needsRestart': false,
    'showBetaChannelPromotion': false,
    'showDevChannelPromotion': false,
    'showOwnerWarning': false,
    'showSystemFlagsLink': true,
  };

  constructor() {
    super([
      'restartBrowser',
      // <if expr="is_chromeos">
      'crosUrlFlagsRedirect',
      // </if>
      'resetAllFlags',
      'requestExperimentalFeatures',
      'enableExperimentalFeature',
      'selectExperimentalFeature',
      'setOriginListFlag',
      'setStringFlag',
    ]);
  }

  setFeatureData(data: ExperimentalFeaturesData) {
    this.featureData = data;
  }

  restartBrowser() {
    this.methodCalled('restartBrowser');
  }

  // <if expr="is_chromeos">
  crosUrlFlagsRedirect() {
    this.methodCalled('crosUrlFlagsRedirect');
  }
  // </if>

  resetAllFlags() {
    this.methodCalled('resetAllFlags');
  }

  requestExperimentalFeatures() {
    this.methodCalled('requestExperimentalFeatures');
    return Promise.resolve(structuredClone(this.featureData));
  }

  enableExperimentalFeature(internalName: string, enable: boolean) {
    this.methodCalled('enableExperimentalFeature', internalName, enable);
  }

  selectExperimentalFeature(internalName: string, index: number) {
    this.methodCalled('selectExperimentalFeature', internalName, index);
  }

  setOriginListFlag(internalName: string, value: string) {
    this.methodCalled('setOriginListFlag', internalName, value);
  }

  setStringFlag(internalName: string, value: string) {
    this.methodCalled('setStringFlag', internalName, value);
  }
}
