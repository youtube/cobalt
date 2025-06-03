// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-cookies-fragment' is the fragment in a privacy
 * guide card that contains the cookie settings and their descriptions.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';
import '/shared/settings/controls/settings_radio_group.js';
import '../../privacy_page/collapse_radio_button.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {CookiePrimarySetting} from '../../site_settings/site_settings_prefs_browser_proxy.js';

import {getTemplate} from './privacy_guide_cookies_fragment.html.js';

const PrivacyGuideCookiesFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyGuideCookiesFragmentElement extends
    PrivacyGuideCookiesFragmentBase {
  static get is() {
    return 'privacy-guide-cookies-fragment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Primary cookie control states for use in bindings.
       */
      cookiePrimarySettingEnum_: {
        type: Object,
        value: CookiePrimarySetting,
      },
    };
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private startStateBlock3PIncognito_: boolean;

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }

  private onViewEnterStart_() {
    this.startStateBlock3PIncognito_ =
        this.getPref('generated.cookie_primary_setting').value ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.COOKIES_REACHED);
  }

  private onViewExitFinish_() {
    const endStateBlock3PIncognito =
        this.getPref('generated.cookie_primary_setting').value ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateBlock3PIncognito_) {
      state = endStateBlock3PIncognito ?
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO :
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P;
    } else {
      state = endStateBlock3PIncognito ?
          PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO :
          PrivacyGuideSettingsStates.BLOCK_3P_TO_3P;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }

  private onCookies3pIncognitoClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
  }

  private onCookies3pClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeCookiesBlock3P');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-cookies-fragment': PrivacyGuideCookiesFragmentElement;
  }
}
customElements.define(
    PrivacyGuideCookiesFragmentElement.is, PrivacyGuideCookiesFragmentElement);
