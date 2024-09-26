// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './advanced_options_settings.js';
import './button_strip.js';
import './color_settings.js';
import './copies_settings.js';
import './dpi_settings.js';
import './duplex_settings.js';
import './header.js';
import './layout_settings.js';
import './media_size_settings.js';
import './margins_settings.js';
import './more_settings.js';
import './other_options_settings.js';
import './pages_per_sheet_settings.js';
import './pages_settings.js';
// <if expr="is_chromeos">
import './pin_settings.js';
// </if>
import './print_preview_vars.css.js';
import './scaling_settings.js';
import '../strings.m.js';
// <if expr="not is_chromeos">
import './link_container.js';

// </if>

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DarkModeMixin} from '../dark_mode_mixin.js';
import {Destination} from '../data/destination.js';
import {Error, State} from '../data/state.js';
import {MetricsContext, PrintSettingsUiBucket} from '../metrics.js';

import {DestinationState, PrintPreviewDestinationSettingsElement} from './destination_settings.js';
import {SettingsMixin} from './settings_mixin.js';
import {getTemplate} from './sidebar.html.js';

/**
 * Number of settings sections to show when "More settings" is collapsed.
 */
const MAX_SECTIONS_TO_SHOW: number = 6;

export interface PrintPreviewSidebarElement {
  $: {
    destinationSettings: PrintPreviewDestinationSettingsElement,
  };
}

const PrintPreviewSidebarElementBase = CrContainerShadowMixin(
    WebUiListenerMixin(SettingsMixin(DarkModeMixin(PolymerElement))));

export class PrintPreviewSidebarElement extends PrintPreviewSidebarElementBase {
  static get is() {
    return 'print-preview-sidebar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      controlsManaged: Boolean,

      destination: {
        type: Object,
        notify: true,
      },

      destinationState: {
        type: Number,
        notify: true,
      },

      error: {
        type: Number,
        notify: true,
      },

      isPdf: Boolean,

      pageCount: Number,

      state: {
        type: Number,
        observer: 'onStateChanged_',
      },

      controlsDisabled_: {
        type: Boolean,
        computed: 'computeControlsDisabled_(state)',
      },

      maxSheets: Number,

      sheetCount_: {
        type: Number,
        computed: 'computeSheetCount_(' +
            'settings.pages.*, settings.duplex.*, settings.copies.*)',
      },

      firstLoad_: {
        type: Boolean,
        value: true,
      },

      isInAppKioskMode_: {
        type: Boolean,
        value: false,
      },

      settingsExpandedByUser_: {
        type: Boolean,
        value: false,
      },

      shouldShowMoreSettings_: {
        type: Boolean,
        computed: 'computeShouldShowMoreSettings_(settings.pages.available, ' +
            'settings.copies.available, settings.layout.available, ' +
            'settings.color.available, settings.mediaSize.available, ' +
            'settings.dpi.available, settings.margins.available, ' +
            'settings.pagesPerSheet.available, settings.scaling.available, ' +
            'settings.duplex.available, settings.otherOptions.available, ' +
            'settings.vendorItems.available)',
      },
    };
  }

  controlsManaged: boolean;
  destination: Destination|null;
  destinationState: DestinationState;
  error: Error;
  isPdf: boolean;
  pageCount: number;
  state: State;
  private controlsDisabled_: boolean;
  private firstLoad_: boolean;
  private isInAppKioskMode_: boolean;
  private settingsExpandedByUser_: boolean;
  private sheetCount_: number;
  private shouldShowMoreSettings_: boolean;

  /**
   * @param defaultPrinter The system default printer ID.
   * @param serializedDestinationSelectionRulesStr String with rules
   *     for selecting the default destination.
   * @param pdfPrinterDisabled Whether the PDF printer is disabled.
   * @param isDriveMounted Whether Google Drive is mounted. Only used
        on Chrome OS.
   */
  init(
      appKioskMode: boolean, defaultPrinter: string,
      serializedDestinationSelectionRulesStr: string|null,
      pdfPrinterDisabled: boolean, isDriveMounted: boolean) {
    this.isInAppKioskMode_ = appKioskMode;
    pdfPrinterDisabled = this.isInAppKioskMode_ || pdfPrinterDisabled;
    // If PDF printing is disabled, then Save to Drive also needs to be disabled
    // on Chrome OS.
    isDriveMounted = !pdfPrinterDisabled && isDriveMounted;
    this.$.destinationSettings.init(
        defaultPrinter, pdfPrinterDisabled, isDriveMounted,
        serializedDestinationSelectionRulesStr);
  }

  /**
   * @return Whether the controls should be disabled.
   */
  private computeControlsDisabled_(): boolean {
    return this.state !== State.READY;
  }

  /**
   * @return The number of sheets that will be printed.
   */
  private computeSheetCount_(): number {
    let sheets = (this.getSettingValue('pages') as number[]).length;
    if (this.getSettingValue('duplex')) {
      sheets = Math.ceil(sheets / 2);
    }
    return sheets * (this.getSettingValue('copies') as number);
  }

  /**
   * @return Whether to show the "More settings" link.
   */
  private computeShouldShowMoreSettings_(): boolean {
    // Destination settings is always available. See if the total number of
    // available sections exceeds the maximum number to show.
    return [
      'pages',
      'copies',
      'layout',
      'color',
      'mediaSize',
      'margins',
      'color',
      'pagesPerSheet',
      'scaling',
      'dpi',
      'duplex',
      'otherOptions',
      'vendorItems',
    ].reduce((count, setting) => {
      return this.getSetting(setting).available ? count + 1 : count;
    }, 1) > MAX_SECTIONS_TO_SHOW;
  }

  /**
   * @return Whether the "more settings" collapse should be expanded.
   */
  private shouldExpandSettings_(): boolean {
    if (this.settingsExpandedByUser_ === undefined ||
        this.shouldShowMoreSettings_ === undefined) {
      return false;
    }

    // Expand the settings if the user has requested them expanded or if more
    // settings is not displayed (i.e. less than 6 total settings available).
    return this.settingsExpandedByUser_ || !this.shouldShowMoreSettings_;
  }

  private onPrintButtonFocused_() {
    this.firstLoad_ = false;
  }

  private onStateChanged_() {
    if (this.state !== State.PRINTING) {
      return;
    }

    if (this.shouldShowMoreSettings_) {
      MetricsContext.printSettingsUi().record(
          this.settingsExpandedByUser_ ?
              PrintSettingsUiBucket.PRINT_WITH_SETTINGS_EXPANDED :
              PrintSettingsUiBucket.PRINT_WITH_SETTINGS_COLLAPSED);
    }
  }

  // <if expr="not is_chromeos">
  /** @return Whether the system dialog link is available. */
  systemDialogLinkAvailable(): boolean {
    const linkContainer =
        this.shadowRoot!.querySelector('print-preview-link-container');
    return !!linkContainer && linkContainer.systemDialogLinkAvailable();
  }
  // </if>

  // <if expr="is_chromeos">
  /**
   * Returns true if at least one non-PDF printer destination is shown in the
   * destination dropdown.
   */
  printerExistsInDisplayedDestinations(): boolean {
    return this.$.destinationSettings.printerExistsInDisplayedDestinations();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-sidebar': PrintPreviewSidebarElement;
  }
}

customElements.define(
    PrintPreviewSidebarElement.is, PrintPreviewSidebarElement);
