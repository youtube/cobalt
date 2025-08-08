// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';
import './share_password_group_avatar.js';
import '../site_favicon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../password_manager_proxy.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getTemplate} from './share_password_confirmation_dialog.html.js';

export interface SharePasswordConfirmationDialogElement {
  $: {
    header: HTMLElement,
    cancel: HTMLElement,
    done: HTMLElement,
    senderAvatar: HTMLImageElement,
    recipientAvatar: HTMLElement,
    description: HTMLElement,
    footerDescription: HTMLElement,
  };
}

// Five seconds in milliseconds.
const FIVE_SECONDS = 5000;

enum ConfirmationDialogStage {
  LOADING,
  CANCELED,
  SUCCESS,
}

const SharePasswordConfirmationDialogElementBase =
    UserUtilMixin(I18nMixin(PolymerElement));

export class SharePasswordConfirmationDialogElement extends
    SharePasswordConfirmationDialogElementBase {
  static get is() {
    return 'share-password-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogStage_: Number,

      password: Object,
      passwordName: String,
      iconUrl: String,

      recipients: {
        type: Array,
        value: [],
      },

      dialogStageEnum_: {
        type: Object,
        value: ConfirmationDialogStage,
        readOnly: true,
      },
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  passwordName: string;
  iconUrl: string;
  recipients: chrome.passwordsPrivate.RecipientInfo[];
  private dialogStage_: ConfirmationDialogStage =
      ConfirmationDialogStage.LOADING;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override ready() {
    super.ready();

    // The user has 5 seconds to cancel the share action while loading/sharing
    // animation is in progress.
    setTimeout(() => {
      if (this.isStage_(ConfirmationDialogStage.CANCELED)) {
        return;
      }
      this.passwordManager_.sharePassword(this.password.id, this.recipients);
      this.dialogStage_ = ConfirmationDialogStage.SUCCESS;
    }, FIVE_SECONDS);
  }

  private isStage_(stage: ConfirmationDialogStage): boolean {
    return this.dialogStage_ === stage;
  }


  private getDialogTitle_(): string {
    switch (this.dialogStage_) {
      case ConfirmationDialogStage.LOADING:
        return this.i18n('shareDialogLoadingTitle');
      case ConfirmationDialogStage.CANCELED:
        return this.i18n('shareDialogCanceledTitle');
      case ConfirmationDialogStage.SUCCESS:
        return this.i18n('shareDialogSuccessTitle');
      default:
        assertNotReached();
    }
  }

  private getSuccessDescription_(): TrustedHTML {
    if (this.recipients.length > 1) {
      return this.i18nAdvanced(
          'sharePasswordConfirmationDescriptionMultipleRecipients', {
            substitutions: [
              this.passwordName,
              this.i18n('passwordSharingLearnMoreURL'),
            ],
          });
    }
    return this.i18nAdvanced(
        'sharePasswordConfirmationDescriptionSingleRecipient', {
          substitutions: [
            this.recipients[0].displayName,
            this.passwordName,
            this.i18n('passwordSharingLearnMoreURL'),
          ],
        });
  }

  private getFooterDescription_(): TrustedHTML {
    // Only for Android Apps that don't have affiliated website, change password
    // url can't be generated.
    if (!this.password.changePasswordUrl) {
      return this.i18nAdvanced('sharePasswordConfirmationFooterAndroidApp');
    }
    return this.i18nAdvanced('sharePasswordConfirmationFooterWebsite', {
      substitutions: [
        this.password.changePasswordUrl,
        this.passwordName,
      ],
    });
  }

  private onDescriptionClick_(e: Event) {
    const element = e.target as HTMLElement;
    if (element.tagName === 'A') {
      recordPasswordSharingInteraction(
          PasswordSharingActions.CONFIRMATION_DIALOG_LEARN_MORE_CLICKED);
    }
  }

  private onFooterClick_(e: Event) {
    const element = e.target as HTMLElement;
    if (element.tagName === 'A') {
      recordPasswordSharingInteraction(
          PasswordSharingActions.CONFIRMATION_DIALOG_CHANGE_PASSWORD_CLICKED);
    }
  }

  private onClickDone_() {
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onClickCancel_() {
    // Ignore the click if a race with the 5 second timeout occurred.
    if (this.isStage_(ConfirmationDialogStage.SUCCESS)) {
      return;
    }
    recordPasswordSharingInteraction(
        PasswordSharingActions.CONFIRMATION_DIALOG_SHARING_CANCELED);
    this.dialogStage_ = ConfirmationDialogStage.CANCELED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-confirmation-dialog':
        SharePasswordConfirmationDialogElement;
  }
}

customElements.define(
    SharePasswordConfirmationDialogElement.is,
    SharePasswordConfirmationDialogElement);
