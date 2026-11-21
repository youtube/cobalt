// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-banner' is the banner shown for prompting the user to
 * clear profile settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ResetBrowserProxy} from './reset_browser_proxy.js';
import {ResetBrowserProxyImpl} from './reset_browser_proxy.js';
import {getCss} from './reset_profile_banner.css.js';
import {getHtml} from './reset_profile_banner.html.js';

export interface SettingsResetProfileBannerElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsResetProfileBannerElementBase = I18nMixinLit(CrLitElement);

export class SettingsResetProfileBannerElement extends
    SettingsResetProfileBannerElementBase {
  static get is() {
    return 'settings-reset-profile-banner';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tamperedPrefs: {type: Array},
      showTamperedPrefsList: {type: Boolean},
    };
  }

  accessor tamperedPrefs: string[] = [];
  accessor showTamperedPrefsList: boolean = false;

  private browserProxy_: ResetBrowserProxy =
      ResetBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getTamperedPreferencePaths().then(prefs => {
      if (prefs.length > 0) {
        this.tamperedPrefs = prefs;
        this.showTamperedPrefsList = true;
        this.$.dialog.showModal();
        this.browserProxy_.onShowResetProfileDialog();
      }
    });
  }

  protected onCancel_() {
    this.browserProxy_.onHideResetProfileBanner();
  }

  protected onConfirmClick_() {
    this.$.dialog.close();
    this.browserProxy_.onHideResetProfileBanner();
  }

  protected onLearnMoreClick_() {
    window.open(this.i18n('resetProfileBannerLearnMoreUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-profile-banner': SettingsResetProfileBannerElement;
  }
}

customElements.define(
    SettingsResetProfileBannerElement.is, SettingsResetProfileBannerElement);
