// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';
import '../user_utils_mixin.js';
import './password_preview_item.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import {getTemplate} from './move_passwords_dialog.html.js';

/**
 * This should be kept in sync with the enum in
 * components/password_manager/core/browser/password_manager_metrics_util.h.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
export const MoveToAccountStoreTrigger = {
  SUCCESSFUL_LOGIN_WITH_PROFILE_STORE_PASSWORD: 0,
  EXPLICITLY_TRIGGERED_IN_SETTINGS: 1,
  EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS: 2,
  COUNT: 3,
};

export interface MovePasswordsDialogElement {
  $: {
    accountEmail: HTMLElement,
    avatar: HTMLImageElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    move: CrButtonElement,
  };
}

const MovePasswordsDialogElementBase = UserUtilMixin(PolymerElement);

export class MovePasswordsDialogElement extends MovePasswordsDialogElementBase {
  static get is() {
    return 'move-passwords-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Password groups displayed in the UI.
       */
      passwords: {
        type: Array,
        value: () => [],
      },

      selectedPasswordIds_: {
        type: Array,
        valie: () => [],
      },
    };
  }

  passwords: chrome.passwordsPrivate.PasswordUiEntry[];
  private selectedPasswordIds_: number[];

  override connectedCallback() {
    super.connectedCallback();

    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
        MoveToAccountStoreTrigger
            .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS,
        MoveToAccountStoreTrigger.COUNT);

    this.selectedPasswordIds_ = this.passwords.map(item => item.id);
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(this.selectedPasswordIds_)
        .then(entries => {
          this.passwords = entries;
          this.$.dialog.showModal();
        })
        .catch(() => {
          this.$.dialog.close();
        });
  }

  private onCancel_() {
    this.$.dialog.cancel();
  }

  private onMoveButtonClick_() {
    assert(this.isOptedInForAccountStorage);
    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        this.selectedPasswordIds_);
    this.$.dialog.close();
  }

  private getUrl_(password: chrome.passwordsPrivate.PasswordUiEntry): string {
    assert(password.affiliatedDomains);
    assert(password.affiliatedDomains.length > 0);
    return password.affiliatedDomains[0].name;
  }

  private passwordSelected_() {
    this.selectedPasswordIds_ =
        Array.from(this.shadowRoot!.querySelectorAll('password-preview-item'))
            .filter(item => item.checked)
            .map(item => item.passwordId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-passwords-dialog': MovePasswordsDialogElement;
  }
}

customElements.define(
    MovePasswordsDialogElement.is, MovePasswordsDialogElement);
