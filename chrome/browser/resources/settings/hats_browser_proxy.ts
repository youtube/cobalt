// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

/**
 * All Trust & Safety based interactions which may result in a HaTS survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 */
export enum TrustSafetyInteraction {
  RAN_SAFETY_CHECK = 0,
  USED_PRIVACY_CARD = 1,
  OPENED_PRIVACY_SANDBOX = 2,
  OPENED_PASSWORD_MANAGER = 3,
  COMPLETED_PRIVACY_GUIDE = 4,
  RAN_PASSWORD_CHECK = 5,
  OPENED_AD_PRIVACY = 6,
  OPENED_TOPICS_SUBPAGE = 7,
  OPENED_FLEDGE_SUBPAGE = 8,
  OPENED_AD_MEASUREMENT_SUBPAGE = 9,
}

/**
 * All interactions from the security settings page which may result in a HaTS
 * survey. Must be kept in sync with the enum of the same name located in:
 * chrome/browser/ui/webui/settings/hats_handler.h
 */
export enum SecurityPageInteraction {
  RADIO_BUTTON_ENHANCED_CLICK = 0,
  RADIO_BUTTON_STANDARD_CLICK = 1,
  RADIO_BUTTON_DISABLE_CLICK = 2,
  EXPAND_BUTTON_ENHANCED_CLICK = 3,
  EXPAND_BUTTON_STANDARD_CLICK = 4,
  NO_INTERACTION = 5,
}

/**
 * Enumeration of all safe browsing modes. Must be kept in sync with the enum
 * of the same name located in:
 * chrome/browser/safe_browsing/generated_safe_browsing_pref.h
 */
export enum SafeBrowsingSetting {
  ENHANCED = 0,
  STANDARD = 1,
  DISABLED = 2,
}

/**
 * All interactions from the security settings page which may result in a HaTS
 * survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 */
export interface HatsBrowserProxy {
  /**
   * Inform HaTS that the user performed a Trust & Safety interaction.
   * @param interaction The type of interaction performed by the user.
   */
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction): void;

  /**
   * Inform HaTS that the user performed an interaction on security page.
   * @param securityPageInteraction The type of interaction performed on the
   *     security page.
   * @param safeBrowsingSetting The type of safe browsing settings the user is
   *     on prior to the interaction.
   */
  securityPageInteractionOccurred(
      securityPageInteraction: SecurityPageInteraction,
      safeBrowsingSetting: SafeBrowsingSetting): void;
}

export class HatsBrowserProxyImpl implements HatsBrowserProxy {
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction) {
    chrome.send('trustSafetyInteractionOccurred', [interaction]);
  }

  securityPageInteractionOccurred(
      securityPageInteraction: SecurityPageInteraction,
      safeBrowsingSetting: SafeBrowsingSetting) {
    chrome.send(
        'securityPageInteractionOccurred',
        [securityPageInteraction, safeBrowsingSetting]);
  }

  static getInstance(): HatsBrowserProxy {
    return instance || (instance = new HatsBrowserProxyImpl());
  }

  static setInstance(obj: HatsBrowserProxy) {
    instance = obj;
  }
}

let instance: HatsBrowserProxy|null = null;
