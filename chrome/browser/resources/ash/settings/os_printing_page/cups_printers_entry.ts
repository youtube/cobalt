// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry' is a component that holds a
 * printer.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import './cups_printer_types.js';

import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {PrinterSettingsUserAction, recordPrinterSettingsUserAction} from './cups_printers.js';
import {getTemplate} from './cups_printers_entry.html.js';
import {computePrinterState, PrinterState, PrinterStatusReason, STATUS_REASON_STRING_KEY_MAP} from './printer_status.js';

const SettingsCupsPrintersEntryElementBase =
    FocusRowMixin(I18nMixin(PolymerElement));

export class SettingsCupsPrintersEntryElement extends
    SettingsCupsPrintersEntryElementBase {
  static get is() {
    return 'settings-cups-printers-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      printerEntry: Object,

      /**
       * This value is set to true if the printer is in saving mode.
       */
      savingPrinter: Boolean,

      /**
       * This value is set to true if UserPrintersAllowed policy is enabled.
       */
      userPrintersAllowed: {
        type: Boolean,
        value: false,
      },

      /**
       * The cache of printer status reasons used to look up this entry's
       * current printer status. Populated and maintained by
       * cups_saved_printers.ts.
       */
      printerStatusReasonCache: Map<string, PrinterStatusReason>,

      hasHighSeverityError_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Number of printers in the respective list this row is part of. */
      numPrinters: Number,

      /**
       * True when the "printer-settings-printer-status" feature flag is
       * enabled.
       */
      isPrinterSettingsPrinterStatusEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean(
              'isPrinterSettingsPrinterStatusEnabled');
        },
        readOnly: true,
      },

      /**
       * True when the "printer-settings-revamp" feature flag is enabled.
       */
      isPrinterSettingsRevampEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isPrinterSettingsRevampEnabled');
        },
        readOnly: true,
        reflectToAttribute: true,
      },

      /**
       * True when the "print-preview-discovered-printers" feature flag is
       * enabled.
       */
      isPrintPreviewDiscoveredPrintersEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean(
              'isPrintPreviewDiscoveredPrintersEnabled');
        },
        readOnly: true,
      },

      /** @protected {boolean} */
      isJellyEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isJellyEnabled');
        },
        readOnly: true,
      },
    };
  }

  printerEntry: PrinterListEntry;
  savingPrinter: boolean;
  userPrintersAllowed: boolean;
  printerStatusReasonCache: Map<string, PrinterStatusReason>;
  numPrinters: number;
  private hasHighSeverityError_: boolean;
  private isPrinterSettingsRevampEnabled_: boolean;
  private isPrinterSettingsPrinterStatusEnabled_: boolean;
  private isPrintPreviewDiscoveredPrintersEnabled_: boolean;
  private isJellyEnabled_: boolean;

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the printer and where the menu should appear.
   */
  private onOpenActionMenuClick_(
      e: CustomEvent<{target: HTMLElement, item: PrinterListEntry}>): void {
    const openActionMenuEvent = new CustomEvent('open-action-menu', {
      bubbles: true,
      composed: true,
      detail: {
        target: e.target,
        item: this.printerEntry,
      },
    });
    this.dispatchEvent(openActionMenuEvent);
  }

  private onAddDiscoveredPrinterClick_(): void {
    const queryDiscoveredPrinterEvent =
        new CustomEvent('query-discovered-printer', {
          bubbles: true,
          composed: true,
          detail: {item: this.printerEntry},
        });
    this.dispatchEvent(queryDiscoveredPrinterEvent);
    recordPrinterSettingsUserAction(PrinterSettingsUserAction.SAVE_PRINTER);
  }

  private onAddAutomaticPrinterClick_(): void {
    const addAutomaticPrinterEvent = new CustomEvent('add-automatic-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addAutomaticPrinterEvent);
    recordPrinterSettingsUserAction(PrinterSettingsUserAction.SAVE_PRINTER);
  }

  private onAddServerPrinterClick_(): void {
    const addPrintServer = new CustomEvent('add-print-server-printer', {
      bubbles: true,
      composed: true,
      detail: {item: this.printerEntry},
    });
    this.dispatchEvent(addPrintServer);
  }

  private showActionsMenu_(): boolean {
    return this.printerEntry.printerType === PrinterType.SAVED ||
        this.printerEntry.printerType === PrinterType.ENTERPRISE;
  }

  private isDiscoveredPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.DISCOVERED;
  }

  private isAutomaticPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.AUTOMATIC;
  }

  private isPrintServerPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.PRINTSERVER;
  }

  private isSavedPrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.SAVED;
  }

  private isEnterprisePrinter_(): boolean {
    return this.printerEntry.printerType === PrinterType.ENTERPRISE;
  }

  private isConfigureDisabled_(): boolean {
    return !this.userPrintersAllowed || this.savingPrinter;
  }

  private getSaveButtonAria_(): string {
    return loadTimeData.getStringF(
        'savePrinterAria', this.printerEntry.printerInfo.printerName);
  }

  private getSetupButtonAria_(): string {
    return loadTimeData.getStringF(
        'setupPrinterAria', this.printerEntry.printerInfo.printerName);
  }

  // The standard printer icon shows for printer entries classified as nearby
  // printers. An exception is enterprise printers which display the managed
  // icon.
  private showNearbyPrinterIcon_(): boolean {
    return !this.isSavedPrinter_() && !this.isEnterprisePrinter_() &&
        this.isPrinterSettingsRevampEnabled_;
  }

  // Printer status icons are only shown for saved printers.
  private showPrinterStatusIcon_(): boolean {
    return this.isSavedPrinter_() &&
        this.isPrinterSettingsPrinterStatusEnabled_;
  }

  private showPrinterIcon_(): boolean {
    return this.showNearbyPrinterIcon_() || this.showPrinterStatusIcon_();
  }

  // True if a printer or managed icon needs to be shown.
  // TODO(b/278621575): Remove this function once Printer Settings Revamp flag
  // is enabled because every entry will show either a printer or managed icon.
  private showAnyIcon_(): boolean {
    return this.showPrinterIcon_() || this.printerEntry.printerInfo.isManaged;
  }

  private getPrinterIcon_(): string {
    const printerStatusIcon = this.isJellyEnabled_ ?
        `os-settings:printer-status-illo` :
        `os-settings:printer-status`;

    // Only saved printers need to display an icon with printer status.
    if (!this.isSavedPrinter_()) {
      return 'os-settings:printer-plain';
    }

    const printerStatusReason = this.printerStatusReasonCache.get(
        this.printerEntry.printerInfo.printerId);
    if (printerStatusReason === undefined || printerStatusReason === null) {
      return `${printerStatusIcon}-grey`;
    }

    let iconColor = '';
    switch (computePrinterState(printerStatusReason)) {
      case PrinterState.GOOD:
        iconColor = 'green';
        break;
      case PrinterState.LOW_SEVERITY_ERROR:
        iconColor = 'orange';
        break;
      case PrinterState.HIGH_SEVERITY_ERROR:
        iconColor = 'red';
        break;
      case PrinterState.UNKNOWN:
        iconColor = 'grey';
        break;
      default:
        assertNotReached('Invalid PrinterState');
    }
    return `${printerStatusIcon}-${iconColor}`;
  }

  private getStatusReasonString_(): TrustedHTML {
    // Only saved printers need to display printer status text.
    if (!this.isSavedPrinter_()) {
      return window.trustedTypes!.emptyHTML;
    }

    const printerStatusReason = this.printerStatusReasonCache.get(
        this.printerEntry.printerInfo.printerId);
    if (!printerStatusReason) {
      return window.trustedTypes!.emptyHTML;
    }

    // Use the printer state to determine printer status text color.
    this.hasHighSeverityError_ = computePrinterState(printerStatusReason) ===
        PrinterState.HIGH_SEVERITY_ERROR;

    // Use the printer state to determine printer status text content.
    const statusReasonStringKey =
        STATUS_REASON_STRING_KEY_MAP.get(printerStatusReason);
    return statusReasonStringKey ? this.i18nAdvanced(statusReasonStringKey) :
                                   window.trustedTypes!.emptyHTML;
  }

  private getAriaLabel_(): string {
    if (!this.printerEntry) {
      return '';
    }

    return this.i18n(
        'printerEntryAriaLabel', this.printerEntry.printerInfo.printerName,
        this.getStatusReasonString_().toString(), this.focusRowIndex + 1,
        this.numPrinters);
  }

  // When the "print-preview-discovered-printers" flag is enabled, discovered
  // printers should be treated like automatic printers. So show "Save" button
  // instead of "Setup".
  private getSetupButtonLabel_(): string {
    return this.i18n(
        this.isPrintPreviewDiscoveredPrintersEnabled_ ? 'savePrinter' :
                                                        'setupPrinter');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-printers-entry': SettingsCupsPrintersEntryElement;
  }
}

customElements.define(
    SettingsCupsPrintersEntryElement.is, SettingsCupsPrintersEntryElement);
