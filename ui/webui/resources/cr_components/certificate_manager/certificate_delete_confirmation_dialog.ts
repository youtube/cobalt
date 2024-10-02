// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A confirmation dialog allowing the user to delete various types
 * of certificates.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './certificate_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_delete_confirmation_dialog.html.js';
import {CertificatesBrowserProxyImpl, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

export interface CertificateDeleteConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: HTMLElement,
  };
}

const CertificateDeleteConfirmationDialogElementBase =
    I18nMixin(PolymerElement);

export class CertificateDeleteConfirmationDialogElement extends
    CertificateDeleteConfirmationDialogElementBase {
  static get is() {
    return 'certificate-delete-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      certificateType: String,
    };
  }

  model: CertificateSubnode;
  certificateType: CertificateType;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private getTitleText_(): string {
    const getString = (localizedMessageId: string) =>
        loadTimeData.getStringF(localizedMessageId, this.model.name);

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserTitle');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerTitle');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaTitle');
      case CertificateType.OTHER:
        return getString('certificateManagerDeleteOtherTitle');
      default:
        assertNotReached();
    }
  }

  private getDescriptionText_(): string {
    const getString = loadTimeData.getString.bind(loadTimeData);
    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return getString('certificateManagerDeleteUserDescription');
      case CertificateType.SERVER:
        return getString('certificateManagerDeleteServerDescription');
      case CertificateType.CA:
        return getString('certificateManagerDeleteCaDescription');
      case CertificateType.OTHER:
        return '';
      default:
        assertNotReached();
    }
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    CertificatesBrowserProxyImpl.getInstance()
        .deleteCertificate(this.model.id)
        .then(
            () => {
              this.$.dialog.close();
            },
            error => {
              if (error === null) {
                return;
              }
              this.$.dialog.close();
              this.dispatchEvent(new CustomEvent('certificates-error', {
                bubbles: true,
                composed: true,
                detail: {error: error, anchor: null},
              }));
            });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-delete-confirmation-dialog':
        CertificateDeleteConfirmationDialogElement;
  }
}

customElements.define(
    CertificateDeleteConfirmationDialogElement.is,
    CertificateDeleteConfirmationDialogElement);
