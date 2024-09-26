// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// <if expr="is_chromeos">
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
// </if>
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, PrinterType} from '../data/destination.js';
import {State} from '../data/state.js';

import {getTemplate} from './button_strip.html.js';


export class PrintPreviewButtonStripElement extends PolymerElement {
  static get is() {
    return 'print-preview-button-strip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,

      firstLoad: Boolean,

      maxSheets: Number,

      sheetCount: Number,

      state: Number,

      printButtonEnabled_: {
        type: Boolean,
        value: false,
      },

      printButtonLabel_: {
        type: String,
        value() {
          return loadTimeData.getString('printButton');
        },
      },

      // <if expr="is_chromeos">
      errorMessage_: {
        type: String,
        observer: 'errorMessageChanged_',
      },
      // </if>
    };
  }

  static get observers() {
    return [
      'updatePrintButtonLabel_(destination.id)',
      'updatePrintButtonEnabled_(state, destination.id, maxSheets, sheetCount)',
      // <if expr="is_chromeos">
      'updateErrorMessage_(state, destination.id, maxSheets, sheetCount)',
      // </if>

    ];
  }

  destination: Destination;
  firstLoad: boolean;
  maxSheets: number;
  sheetCount: number;
  state: State;
  private printButtonEnabled_: boolean;
  private printButtonLabel_: string;
  // <if expr="is_chromeos">
  private errorMessage_: string;
  // </if>

  private lastState_: State = State.NOT_READY;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onPrintClick_() {
    this.fire_('print-requested');
  }

  private onCancelClick_() {
    this.fire_('cancel-requested');
  }

  private isPdf_(): boolean {
    return this.destination &&
        this.destination.type === PrinterType.PDF_PRINTER;
  }

  private updatePrintButtonLabel_() {
    this.printButtonLabel_ =
        loadTimeData.getString(this.isPdf_() ? 'saveButton' : 'printButton');
  }

  private updatePrintButtonEnabled_() {
    switch (this.state) {
      case (State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (State.READY):
        // <if expr="is_chromeos">
        this.printButtonEnabled_ = !this.printButtonDisabled_();
        // </if>
        // <if expr="not is_chromeos">
        this.printButtonEnabled_ = true;
        // </if>
        if (this.firstLoad || this.lastState_ === State.PRINTING) {
          this.shadowRoot!
              .querySelector<CrButtonElement>(
                  'cr-button.action-button')!.focus();
          this.fire_('print-button-focused');
        }
        break;
      default:
        this.printButtonEnabled_ = false;
        break;
    }
    this.lastState_ = this.state;
  }

  // <if expr="is_chromeos">
  /**
   * @return Whether to disable "Print" button because of sheets limit policy.
   */
  private printButtonDisabled_(): boolean {
    // The "Print" button is disabled if 3 conditions are met:
    // * This is "real" printing, i.e. not saving to PDF/Drive.
    // * Sheets policy is present.
    // * Either number of sheets is not calculated or exceeds policy limit.
    return !this.isPdf_() && this.maxSheets > 0 &&
        (this.sheetCount === 0 || this.sheetCount > this.maxSheets);
  }

  /**
   * @return Whether to show the "Too many sheets" error.
   */
  private showSheetsError_(): boolean {
    // The error is shown if the number of sheets is already calculated and the
    // print button is disabled.
    return this.sheetCount > 0 && this.printButtonDisabled_();
  }

  private updateErrorMessage_() {
    if (!this.showSheetsError_()) {
      this.errorMessage_ = '';
      return;
    }
    PluralStringProxyImpl.getInstance()
        .getPluralString('sheetsLimitErrorMessage', this.maxSheets)
        .then(label => {
          this.errorMessage_ = label;
        });
  }

  /**
   * Uses IronA11yAnnouncer to notify screen readers that an error is set.
   */
  private errorMessageChanged_() {
    if (this.errorMessage_ !== '') {
      IronA11yAnnouncer.requestAvailability();
      this.fire_('iron-announce', {text: this.errorMessage_});
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-button-strip': PrintPreviewButtonStripElement;
  }
}

customElements.define(
    PrintPreviewButtonStripElement.is, PrintPreviewButtonStripElement);
