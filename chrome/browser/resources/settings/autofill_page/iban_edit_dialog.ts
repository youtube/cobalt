// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-iban-edit-dialog' is the dialog that allows
 * editing or creating an IBAN entry.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../i18n_setup.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './iban_edit_dialog.html.js';
import {PaymentsManagerImpl, PaymentsManagerProxy} from './payments_manager_proxy.js';

declare global {
  interface HTMLElementEventMap {
    'save-iban': CustomEvent<chrome.autofillPrivate.IbanEntry>;
  }
}

export interface SettingsIbanEditDialogElement {
  $: {
    dialog: CrDialogElement,
    valueInput: CrInputElement,
    nicknameInput: CrInputElement,
    cancelButton: CrButtonElement,
    saveButton: CrButtonElement,
  };
}

const SettingsIbanEditDialogElementBase = I18nMixin(PolymerElement);

export class SettingsIbanEditDialogElement extends
    SettingsIbanEditDialogElementBase {
  static get is() {
    return 'settings-iban-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The IBAN being added or edited. Null means add a new IBAN, otherwise,
       * edit the existing IBAN.
       */
      iban: {
        type: Object,
        value: null,
      },

      /**
       * The actual title that's used for this dialog. Will be context sensitive
       * based on which type of IBAN method is being viewed, and if it is being
       * created or edited.
       */
      title_: String,

      value_: String,
      nickname_: String,
    };
  }

  iban: chrome.autofillPrivate.IbanEntry|null;
  private value_?: string;
  private nickname_?: string;
  private title_: string;
  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.iban) {
      // Save IBAN button is by default enabled in 'EDIT' mode as IBAN value is
      // pre-populated.
      this.value_ = this.iban.value;
      this.nickname_ = this.iban.nickname;
      this.title_ = this.i18n('editIbanTitle');
    } else {
      this.title_ = this.i18n('addIbanTitle');
      // Save IBAN button is disabled in 'ADD' mode as IBAN value is empty.
      this.$.saveButton.disabled = true;
    }
    this.$.dialog.showModal();
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for clicking the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * Handler for clicking the save button.
   */
  private onIbanSaveButtonClick_() {
    const iban = {
      guid: this.iban?.guid,
      value: this.value_!.trim(),
      nickname: this.nickname_ ? this.nickname_.trim() : '',
    };
    this.dispatchEvent(new CustomEvent(
        'save-iban', {bubbles: true, composed: true, detail: iban}));
    this.close();
  }

  private updateSaveIbanButtonEnablement_() {
    this.isValidIban().then(isValid => {
      this.$.saveButton.disabled = !isValid;
    });
  }

  private async isValidIban(): Promise<boolean> {
    if (!this.value_) {
      return Promise.resolve(false);
    }
    // The save button is enabled if the value of the IBAN is invalid (after
    // removing all whitespace from it).
    const isValid = await this.paymentsManager_.isValidIban(
        this.value_!.replace(/\s/g, ''));
    return isValid;
  }


  /**
   * @param  nickname of the IBAN, undefined when not set.
   * @return nickname character length.
   */
  private computeNicknameCharCount_(): number {
    return (this.nickname_ || '').length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-iban-edit-dialog': SettingsIbanEditDialogElement;
  }
}

customElements.define(
    SettingsIbanEditDialogElement.is, SettingsIbanEditDialogElement);
