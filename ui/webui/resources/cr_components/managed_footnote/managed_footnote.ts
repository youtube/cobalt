// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating that this user is managed by
 * their organization. This component uses the |isManaged| boolean in
 * loadTimeData, and the |managedByOrg| i18n string.
 *
 * If |isManaged| is false, this component is hidden. If |isManaged| is true, it
 * becomes visible.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './managed_footnote.html.js';

const ManagedFootnoteElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class ManagedFootnoteElement extends ManagedFootnoteElementBase {
  static get is() {
    return 'managed-footnote';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the user is managed by their organization through enterprise
       * policies.
       */
      isManaged_: {
        reflectToAttribute: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
      },

      /**
       * Whether the device should be indicated as managed rather than the
       * browser.
       */
      showDeviceInfo: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isManaged_: boolean;
  showDeviceInfo: boolean;

  override ready() {
    super.ready();
    this.addWebUiListener('is-managed-changed', (managed: boolean) => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  }

  /** @return Message to display to the user. */
  private getManagementString_(): TrustedHTML {
    // <if expr="chromeos_ash">
    if (this.showDeviceInfo) {
      return this.i18nAdvanced('deviceManagedByOrg');
    }
    // </if>
    return this.i18nAdvanced('browserManagedByOrg');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-footnote': ManagedFootnoteElement;
  }
}

customElements.define(ManagedFootnoteElement.is, ManagedFootnoteElement);

chrome.send('observeManagedUI');
