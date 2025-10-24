// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AiPageCompareInteractions, AiPageComposeInteractions, AiPageHistorySearchInteractions, AiPageInteractions, AiPageTabOrganizationInteractions, DeleteBrowsingDataAction, MetricsBrowserProxy, PrivacyElementInteractions, PrivacyGuideInteractions, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached, SafeBrowsingInteractions, SafetyCheckNotificationsModuleInteractions, SafetyCheckUnusedSitePermissionsModuleInteractions, SafetyHubCardState, SafetyHubEntryPoint, SafetyHubModuleType, SafetyHubSurfaces} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordBooleanHistogram',
      'recordSettingsPageHistogram',
      'recordPrivacyGuideFlowLengthHistogram',
      'recordSafeBrowsingInteractionHistogram',
      'recordPrivacyGuideNextNavigationHistogram',
      'recordPrivacyGuideEntryExitHistogram',
      'recordPrivacyGuideSettingsStatesHistogram',
      'recordPrivacyGuideStepsEligibleAndReachedHistogram',
      'recordDeleteBrowsingDataAction',
      'recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram',
      'recordSafetyHubCardStateClicked',
      'recordSafetyHubDashboardAnyWarning',
      'recordSafetyHubEntryPointClicked',
      'recordSafetyHubEntryPointShown',
      'recordSafetyHubImpression',
      'recordSafetyHubInteraction',
      'recordSafetyHubModuleWarningImpression',
      'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram',
      'recordSafetyHubNotificationPermissionsModuleListCountHistogram',
      'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram',
      'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram',
      // <if expr="_google_chrome and is_win">
      'recordFeatureNotificationsChange',
      // </if>
      'recordAiPageInteractions',
      'recordAiPageHistorySearchInteractions',
      'recordAiPageCompareInteractions',
      'recordAiPageComposeInteractions',
      'recordAiPageTabOrganizationInteractions',
    ]);
  }

  recordAction(action: string) {
    this.methodCalled('recordAction', action);
  }

  recordBooleanHistogram(histogramName: string, visible: boolean) {
    this.methodCalled('recordBooleanHistogram', [histogramName, visible]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  recordSafeBrowsingInteractionHistogram(interaction:
                                             SafeBrowsingInteractions) {
    this.methodCalled('recordSafeBrowsingInteractionHistogram', interaction);
  }

  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideNextNavigationHistogram', interaction);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideEntryExitHistogram', interaction);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    this.methodCalled('recordPrivacyGuideSettingsStatesHistogram', state);
  }

  recordPrivacyGuideFlowLengthHistogram(steps: number) {
    this.methodCalled('recordPrivacyGuideFlowLengthHistogram', steps);
  }

  recordPrivacyGuideStepsEligibleAndReachedHistogram(
      status: PrivacyGuideStepsEligibleAndReached) {
    this.methodCalled(
        'recordPrivacyGuideStepsEligibleAndReachedHistogram', status);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction) {
    this.methodCalled('recordDeleteBrowsingDataAction', action);
  }

  recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram',
        interaction);
  }

  recordSafetyHubCardStateClicked(
      histogramName: string, state: SafetyHubCardState) {
    this.methodCalled(
        'recordSafetyHubCardStateClicked', [histogramName, state]);
  }

  recordSafetyHubDashboardAnyWarning(visible: boolean) {
    this.methodCalled('recordSafetyHubDashboardAnyWarning', visible);
  }

  recordSafetyHubEntryPointClicked(page: SafetyHubEntryPoint) {
    this.methodCalled('recordSafetyHubEntryPointClicked', page);
  }

  recordSafetyHubEntryPointShown(page: SafetyHubEntryPoint) {
    this.methodCalled('recordSafetyHubModuleWarningImpression', page);
  }

  recordSafetyHubImpression(surface: SafetyHubSurfaces) {
    this.methodCalled('recordSafetyHubImpression', surface);
  }

  recordSafetyHubInteraction(surface: SafetyHubSurfaces) {
    this.methodCalled('recordSafetyHubInteraction', surface);
  }

  recordSafetyHubModuleWarningImpression(module: SafetyHubModuleType) {
    this.methodCalled('recordSafetyHubModuleWarningImpression', module);
  }

  recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyHubNotificationPermissionsModuleListCountHistogram(suggestions:
                                                                     number) {
    this.methodCalled(
        'recordSafetyHubNotificationPermissionsModuleListCountHistogram',
        suggestions);
  }

  recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(suggestions:
                                                                   number) {
    this.methodCalled(
        'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram',
        suggestions);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean) {
    this.methodCalled('recordFeatureNotificationsChange', enabled);
  }
  // </if>

  recordAiPageInteractions(interaction: AiPageInteractions) {
    this.methodCalled('recordAiPageInteractions', interaction);
  }

  recordAiPageHistorySearchInteractions(interaction:
                                            AiPageHistorySearchInteractions) {
    this.methodCalled('recordAiPageHistorySearchInteractions', interaction);
  }

  recordAiPageCompareInteractions(interaction: AiPageCompareInteractions) {
    this.methodCalled('recordAiPageCompareInteractions', interaction);
  }

  recordAiPageComposeInteractions(interaction: AiPageComposeInteractions) {
    this.methodCalled('recordAiPageComposeInteractions', interaction);
  }

  recordAiPageTabOrganizationInteractions(
      interaction: AiPageTabOrganizationInteractions) {
    this.methodCalled('recordAiPageTabOrganizationInteractions', interaction);
  }
}
