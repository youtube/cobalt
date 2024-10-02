// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './shared_style.css.js';
import './dialogs/edit_password_dialog.js';
import './dialogs/multi_store_delete_password_dialog.js';

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_card.html.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from './password_manager_proxy.js';
import {ShowPasswordMixin} from './show_password_mixin.js';

export type PasswordRemovedEvent =
    CustomEvent<{removedFromStores: chrome.passwordsPrivate.PasswordStoreSet}>;

declare global {
  interface HTMLElementEventMap {
    'password-removed': PasswordRemovedEvent;
  }
}

export interface PasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    copyUsernameButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    domainLabel: HTMLElement,
    editButton: CrButtonElement,
    noteValue: HTMLElement,
    passwordValue: CrInputElement,
    showMore: HTMLAnchorElement,
    showPasswordButton: CrIconButtonElement,
    toast: CrToastElement,
    usernameValue: CrInputElement,
  };
}

const PasswordDetailsCardElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

export class PasswordDetailsCardElement extends PasswordDetailsCardElementBase {
  static get is() {
    return 'password-details-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password: Object,
      toastMessage_: String,

      showNoteFully_: Boolean,

      showEditPasswordDialog_: Boolean,
      showDeletePasswordDialog_: Boolean,
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  private toastMessage_: string;
  private noteRows_: number;
  private showNoteFully_: boolean;
  private showEditPasswordDialog_: boolean;
  private showDeletePasswordDialog_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    if (this.isFederated_()) {
      return;
    }
    // Set default value here so listeners can be updated properly.
    this.showNoteFully_ = false;
  }

  private isFederated_(): boolean {
    return !!this.password.federationText;
  }

  private getPasswordLabel_() {
    return this.isFederated_() ? this.i18n('federationLabel') :
                                 this.i18n('passwordLabel');
  }

  private getPasswordValue_(): string|undefined {
    return this.isFederated_() ? this.password.federationText :
                                 this.password.password;
  }

  private getPasswordType_(): string {
    return this.isFederated_() ? 'text' : this.getPasswordInputType();
  }

  private onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.password.id, chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => this.showToast_(this.i18n('passwordCopiedToClipboard')))
        .catch(() => {});
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED);
  }

  private onShowPasswordClick_() {
    this.onShowHidePasswordButtonClick();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED);
  }

  private onCopyUsernameClick_() {
    navigator.clipboard.writeText(this.password.username);
    this.showToast_(this.i18n('usernameCopiedToClipboard'));
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.USERNAME_COPY_BUTTON_CLICKED);
  }

  private onDeleteClick_() {
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_DELETE_BUTTON_CLICKED);
    if (this.password.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT) {
      this.showDeletePasswordDialog_ = true;
      return;
    }
    PasswordManagerImpl.getInstance().removeSavedPassword(
        this.password.id, this.password.storedIn);
    this.dispatchEvent(new CustomEvent('password-removed', {
      bubbles: true,
      composed: true,
      detail: {
        removedFromStores: this.password.storedIn,
      },
    }));
  }

  private showToast_(message: string) {
    this.toastMessage_ = message;
    this.$.toast.show();
  }

  private onEditClicked_() {
    this.showEditPasswordDialog_ = true;
    this.extendAuthValidity_();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSWORD_EDIT_BUTTON_CLICKED);
  }

  private onEditPasswordDialogClosed_() {
    // Only note is notified because updating username or password triggers
    // recomputing of an id which updates the whole list of displayed passwords.
    this.notifyPath('password.note');
    this.showEditPasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  private onDeletePasswordDialogClosed_() {
    this.showDeletePasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  private getNoteValue_(): string {
    return !this.password.note ? this.i18n('emptyNote') : this.password.note!;
  }

  private isNoteFullyVisible_(): boolean {
    return this.showNoteFully_ ||
        this.$.noteValue.scrollHeight === this.$.noteValue.offsetHeight;
  }

  private onshowMoreClick_(e: Event) {
    e.preventDefault();
    this.showNoteFully_ = true;
    this.extendAuthValidity_();
  }

  private extendAuthValidity_() {
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }

  private getDomainLabel_(): string {
    const hasApps = this.password.affiliatedDomains?.some(
        domain => domain.signonRealm.startsWith('android://'));
    const hasSites = this.password.affiliatedDomains?.some(
        domain => !domain.signonRealm.startsWith('android://'));
    if (hasApps && hasSites) {
      return this.i18n('sitesAndAppsLabel');
    }
    return hasApps ? this.i18n('appsLabel') : this.i18n('sitesLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
