// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a cellular APN in the APN list.
 */

import './network_shared.css.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {ApnDetailDialogMode, ApnEventData, getApnDisplayName} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {ApnProperties, ApnState, CrosNetworkConfigInterface} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {getTemplate} from './apn_list_item.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnListItemBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ApnListItem extends ApnListItemBase {
  static get is() {
    return 'apn-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The GUID of the network to display details for. */
      guid: String,

      /**@type {!ApnProperties}*/
      apn: {type: Object},

      isConnected: {
        type: Boolean,
        value: false,
      },

      shouldDisallowDisablingRemoving: {
        type: Boolean,
        value: false,
      },

      shouldDisallowEnabling: {
        type: Boolean,
        value: false,
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      /** @private */
      isDisabled_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDisabled_(apn)',
      },
    };
  }

  constructor() {
    super();
    /** @private {!CrosNetworkConfigInterface} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * @param {!ApnProperties} apn
   * @private
   */
  getApnDisplayName_(apn) {
    return getApnDisplayName(this.i18n.bind(this), apn);
  }

  /**
   * Opens the three dots menu.
   * @private
   */
  onMenuButtonClicked_(event) {
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu)
        .showAt(/** @type {!HTMLElement} */ (event.target));
  }

  /** @private */
  closeMenu_() {
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  }

  /**
   * Opens APN Details dialog.
   * @private
   */
  onDetailsClicked_() {
    assert(!!this.apn);
    this.closeMenu_();
    this.dispatchEvent(new CustomEvent('show-apn-detail-dialog', {
      composed: true,
      bubbles: true,
      detail: /** @type {!ApnEventData} */ ({
        apn: this.apn,
        // Only allow editing if the APN is a custom APN.
        mode: this.apn.id ? ApnDetailDialogMode.EDIT : ApnDetailDialogMode.VIEW,
      }),
    }));
  }

  /**
   * Disables the selected APN.
   * @private
   */
  onDisableClicked_() {
    assert(this.guid);
    assert(this.apn);
    this.closeMenu_();
    if (!this.apn.id) {
      console.error('Only custom APNs can be disabled.');
      return;
    }

    if (this.apn.state !== ApnState.kEnabled) {
      console.error('Only an APN that is enabled can be disabled.');
      return;
    }

    if (this.shouldDisallowDisablingRemoving) {
      this.dispatchEvent(new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('apnWarningPromptForDisableRemove'),
      }));
      return;
    }

    const apn =
        /** @type {!ApnProperties} */ (Object.assign({}, this.apn));
    apn.state = ApnState.kDisabled;
    this.networkConfig_.modifyCustomApn(this.guid, apn);
  }

  /**
   * Enables the selected APN.
   * @private
   */
  onEnableClicked_() {
    assert(this.guid);
    assert(this.apn);
    this.closeMenu_();
    if (!this.apn.id) {
      console.error('Only custom APNs can be enabled.');
      return;
    }

    if (this.apn.state !== ApnState.kDisabled) {
      console.error('Only an APN that is disabled can be enabled.');
      return;
    }

    if (this.shouldDisallowEnabling) {
      this.dispatchEvent(new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('apnWarningPromptForEnable'),
      }));
      return;
    }

    const apn =
        /** @type {!ApnProperties} */ (Object.assign({}, this.apn));
    apn.state = ApnState.kEnabled;
    this.networkConfig_.modifyCustomApn(this.guid, apn);
  }

  /**
   * Removes the selected APN.
   * @private
   */
  onRemoveClicked_() {
    assert(this.guid);
    assert(this.apn);
    this.closeMenu_();
    if (!this.apn.id) {
      console.error('Only custom APNs can be removed.');
      return;
    }

    if (this.shouldDisallowDisablingRemoving) {
      this.dispatchEvent(new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('apnWarningPromptForDisableRemove'),
      }));
      return;
    }

    this.networkConfig_.removeCustomApn(
        this.guid, /** @type {string} */ (this.apn.id));
  }

  /**
   * Returns true if disable menu button should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowDisableMenuItem_() {
    return !!this.apn.id && this.apn.state === ApnState.kEnabled;
  }

  /**
   * Returns true if enable menu button should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowEnableMenuItem_() {
    return !!this.apn.id && this.apn.state === ApnState.kDisabled;
  }

  /**
   * Returns true if remove menu button should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowRemoveMenuItem_() {
    return !!this.apn.id;
  }

  /**
   * Returns true if the apn is disabled.
   * @return {boolean}
   * @private
   */
  computeIsDisabled_() {
    return !!this.apn.id && this.apn.state === ApnState.kDisabled;
  }

  /**
   * Returns the label for the "Details" menu item.
   * @return {string}
   * @private
   */
  getDetailsMenuItemLabel_() {
    return this.apn.id ? this.i18n('apnMenuEdit') : this.i18n('apnMenuDetails');
  }

  /**
   * Returns accessibility label for the item.
   * @return {string}
   * @private
   */
  getAriaLabel_() {
    if (!this.apn) {
      return '';
    }

    let a11yLabel = this.i18n(
        'apnA11yName', this.itemIndex + 1, this.listSize,
        this.getApnDisplayName_(this.apn));

    if (!this.apn.id) {
      a11yLabel += ' ' + this.i18n('apnA11yAutoDetected');
    }

    if (this.isConnected) {
      a11yLabel += ' ' + this.i18n('apnA11yConnected');
    }

    if (this.isDisabled_) {
      a11yLabel += ' ' + this.i18n('apnA11yDisabled');
    }

    return a11yLabel;
  }
}

customElements.define(ApnListItem.is, ApnListItem);