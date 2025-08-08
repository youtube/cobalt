// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './firmware_confirmation_dialog.js';
import './firmware_update_dialog.js';
import './peripheral_updates_list.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './firmware_update_app.html.js';

/**
 * @fileoverview
 * 'firmware-update-app' is the main landing page for the firmware
 * update app.
 */

const FirmwareUpdateAppElementBase = I18nMixin(PolymerElement);

export class FirmwareUpdateAppElement extends FirmwareUpdateAppElementBase {
  static get is() {
    return 'firmware-update-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    if (loadTimeData.getBoolean('isJellyEnabledForFirmwareUpdate')) {
      // TODO(b/276493795): After the Jelly experiment is launched, replace
      // `cros_styles.css` with `theme/colors.css` directly in `index.html`.
      // Also add `theme/typography.css` to `index.html`.
      document.querySelector('link[href*=\'cros_styles.css\']')
          ?.setAttribute('href', 'chrome://theme/colors.css?sets=legacy,sys');
      const typographyLink = document.createElement('link');
      typographyLink.href = 'chrome://theme/typography.css';
      typographyLink.rel = 'stylesheet';
      document.head.appendChild(typographyLink);
      document.body.classList.add('jelly-enabled');
      /** @suppress {checkTypes} */
      (function() {
        ColorChangeUpdater.forDocument().start();
      })();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FirmwareUpdateAppElement.is]: FirmwareUpdateAppElement;
  }
}

customElements.define(FirmwareUpdateAppElement.is, FirmwareUpdateAppElement);
