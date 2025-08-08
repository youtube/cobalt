// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './safety_hub_module.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyHubBrowserProxy, SafetyHubBrowserProxyImpl} from './safety_hub_browser_proxy.js';
import {getTemplate} from './safety_hub_entry_point.html.js';
import { SettingsSafetyHubModuleElement } from './safety_hub_module.js';
// clang-format on

export interface SettingsSafetyHubEntryPointElement {
  $: {
    button: HTMLElement,
    module: SettingsSafetyHubModuleElement,
  };
}

const SettingsSafetyHubEntryPointElementBase = I18nMixin(PolymerElement);

export class SettingsSafetyHubEntryPointElement extends
    SettingsSafetyHubEntryPointElementBase {
  static get is() {
    return 'settings-safety-hub-entry-point';
  }

  static get template() {
    return getTemplate();
  }


  static get properties() {
    return {
      buttonClass_: {
        type: Boolean,
        computed: 'computeButtonClass_(hasRecommendations_)',
      },

      hasRecommendations_: {
        type: Boolean,
        value: false,
      },

      headerString_: {
        type: String,
        computed: 'computeHeaderString_(hasRecommendations_)',
      },

      subheaderString_: String,

      headerIconColor_: {
        type: String,
        computed: 'computeHeaderIconColor_(hasRecommendations_)',
      },
    };
  }

  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();

  private buttonClass_: string;
  private hasRecommendations_: boolean;
  private headerString_: string;
  private subheaderString_: string;
  private headerIconColor_: string;

  override connectedCallback() {
    super.connectedCallback();
    this.safetyHubBrowserProxy_.getSafetyHubHasRecommendations().then(
        (hasRecommendations: boolean) => {
          this.hasRecommendations_ = hasRecommendations;
        });

    this.safetyHubBrowserProxy_.getSafetyHubEntryPointSubheader().then(
        (subheader: string) => {
          this.subheaderString_ = subheader;
        });
  }

  private computeButtonClass_() {
    return this.hasRecommendations_ ? 'action-button' : '';
  }

  private computeHeaderString_() {
    return this.hasRecommendations_ ? this.i18n('safetyHubEntryPointHeader') :
                                      '';
  }

  private computeHeaderIconColor_() {
    return this.hasRecommendations_ ? 'blue' : '';
  }

  private onClick_() {
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-entry-point': SettingsSafetyHubEntryPointElement;
  }
}

customElements.define(
    SettingsSafetyHubEntryPointElement.is, SettingsSafetyHubEntryPointElement);
