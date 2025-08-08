// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-smart-privacy-subpage' contains smart privacy settings.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/controls/settings_slider.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {SliderTick} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './smart_privacy_subpage.html.js';

/**
 * The values that the quick lock slider can have, in ms.
 */
const QUICK_LOCK_DELAY_MS = [
  30000,
  60000,
  120000,
  180000,
];

/**
 * Formatter for displaying duration text for the slider of quick dim
 * delay.
 */
const secondsFormatter = new Intl.NumberFormat(
    window.navigator.language,
    {style: 'unit', unit: 'second', unitDisplay: 'narrow'});

const SettingsSmartPrivacySubpageBase =
    DeepLinkingMixin(PrefsMixin(RouteObserverMixin(PolymerElement)));

export class SettingsSmartPrivacySubpage extends
    SettingsSmartPrivacySubpageBase {
  static get is() {
    return 'settings-smart-privacy-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the smart privacy page is being rendered in dark mode.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether or not quick dim is enabled.
       */
      isQuickDimEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isQuickDimEnabled');
        },
      },

      /**
       * Text that shows when moving the quick dim delay slider.
       */
      smartPrivacyQuickLockRangeMs_: {
        readOnly: true,
        type: Array,
        value() {
          return QUICK_LOCK_DELAY_MS.map(
              x => ({label: secondsFormatter.format(x / 1000), value: x}));
        },
      },

      /**
       * Whether or not snooping protection is enabled.
       */
      isSnoopingProtectionEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSnoopingProtectionEnabled');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kQuickDim,
          Setting.kSnoopingProtection,
        ]),
      },
    };
  }

  private isDarkModeActive_: boolean;
  private isQuickDimEnabled_: boolean;
  private isSnoopingProtectionEnabled_: boolean;
  private smartPrivacyQuickLockRangeMs_: SliderTick[];

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.SMART_PRIVACY) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns the image source based on whether the smart privacy page is being
   * rendered in dark mode.
   * @returns {string}
   * @private
   */
  private getImageSource_(): string {
    return this.isDarkModeActive_ ?
        'chrome://os-settings/images/smart_privacy_dark.svg' :
        'chrome://os-settings/images/smart_privacy.svg';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSmartPrivacySubpage.is]: SettingsSmartPrivacySubpage;
  }
}

customElements.define(
    SettingsSmartPrivacySubpage.is, SettingsSmartPrivacySubpage);
