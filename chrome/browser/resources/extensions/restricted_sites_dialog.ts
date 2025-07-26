// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './restricted_sites_dialog.html.js';
import {getCss as getSharedStyleCss} from './shared_style.css.js';

export interface ExtensionsRestrictedSitesDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const ExtensionsRestrictedSitesDialogElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsRestrictedSitesDialogElement extends
    ExtensionsRestrictedSitesDialogElementBase {
  static get is() {
    return 'extensions-restricted-sites-dialog';
  }

  static override get styles() {
    return getSharedStyleCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      firstRestrictedSite: {type: String},
    };
  }

  firstRestrictedSite: string = '';

  isOpen(): boolean {
    return this.$.dialog.open;
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onSubmitClick_() {
    this.$.dialog.close();
  }

  protected getDialogTitle_(): string {
    return this.i18n('matchingRestrictedSitesTitle', this.firstRestrictedSite);
  }

  protected getDialogWarning_(): string {
    return this.i18n(
        'matchingRestrictedSitesWarning', this.firstRestrictedSite);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-restricted-sites-dialog':
        ExtensionsRestrictedSitesDialogElement;
  }
}

customElements.define(
    ExtensionsRestrictedSitesDialogElement.is,
    ExtensionsRestrictedSitesDialogElement);
