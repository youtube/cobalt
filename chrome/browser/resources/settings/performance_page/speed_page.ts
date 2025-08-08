// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/controls/settings_radio_group.js';
import '/shared/settings/controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../privacy_page/collapse_radio_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {SettingsRadioGroupElement} from '/shared/settings/controls/settings_radio_group.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsCollapseRadioButtonElement} from '../privacy_page/collapse_radio_button.js';

import {NetworkPredictionOptions} from './constants.js';
import {getTemplate} from './speed_page.html.js';

export interface SpeedPageElement {
  $: {
    preloadingToggle: SettingsToggleButtonElement,
    preloadingExtended: SettingsCollapseRadioButtonElement,
    preloadingRadioGroup: SettingsRadioGroupElement,
    preloadingStandard: SettingsCollapseRadioButtonElement,
  };
}

const SpeedPageElementBase = PrefsMixin(PolymerElement);

export class SpeedPageElement extends SpeedPageElementBase {
  static get is() {
    return 'settings-speed-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Valid network prediction options state. */
      networkPredictionOptionsEnum_: {
        type: Object,
        value: NetworkPredictionOptions,
      },
    };
  }

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      const prefValue = this.getPref<NetworkPredictionOptions>(
                                'net.network_prediction_options')
                            .value;
      if (prefValue === NetworkPredictionOptions.WIFI_ONLY_DEPRECATED) {
        // The default pref value is deprecated, and is treated the same as
        // STANDARD. See chrome/browser/preloading/preloading_prefs.h.
        this.setPrefValue(
            'net.network_prediction_options',
            NetworkPredictionOptions.STANDARD);
      }
    });
  }

  private isPreloadingEnabled_(value: number): boolean {
    return value !== NetworkPredictionOptions.DISABLED;
  }

  private onPreloadingStateChange_() {
    // Automatic expanding is disabled so that the radio buttons are collapsed
    // initially. Because of this, radio buttons' expanded states need to be
    // updated manually.
    this.$.preloadingExtended.updateCollapsed();
    this.$.preloadingStandard.updateCollapsed();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-speed-page': SpeedPageElement;
  }
}

customElements.define(SpeedPageElement.is, SpeedPageElement);
