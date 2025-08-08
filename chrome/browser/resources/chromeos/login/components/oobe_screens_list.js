// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';


/**
 *  A single screen item.
 * @typedef {{
 *   icon: String,
 *   title: String,
 *   screenID: String,
 *   selected: Boolean,
 * }}
 */
export let ScreenItem;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeScreensListBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class OobeScreensList extends OobeScreensListBase {
  static get is() {
    return 'oobe-screens-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of screens to display.
       * @type {!Array<ScreenItem>}
       */
      screensList_: {
        type: Array,
        value: [],
        notify: true,
      },
      /**
       * List of selected screens.
       */
      screensSelected: {
        type: Array,
        value: [],
      },
      /**
       * Number of selected screens.
       */
      selectedScreensCount: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  /**
   * Initialize the list of screens.
   */
  init(screens) {
    this.screensList_ = screens;
    this.screensSelected = [];
    this.selectedScreensCount = 0;
  }

  /**
   * Return the list of selected screens.
   */
  getScreenSelected() {
    return this.screensSelected;
  }

  onClick_(e) {
    const clickedScreen = e.model.screen;
    const previousSelectedState = clickedScreen.selected;
    const curentSelectedState = !previousSelectedState;
    const path =
        `screensList_.${this.screensList_.indexOf(clickedScreen)}.selected`;
    this.set(path, curentSelectedState);
    e.currentTarget.setAttribute('checked', curentSelectedState);

    if (curentSelectedState) {
      this.selectedScreensCount++;
      this.screensSelected.push(clickedScreen.screenID);
    } else {
      this.selectedScreensCount--;
      this.screensSelected.splice(
          this.screensSelected.indexOf(clickedScreen.screenID), 1);
    }
    this.notifyPath('screensList_');
  }

  getSubtitle_(locale, screen_subtitle, screen_id) {
    if (screen_subtitle) {
      // display size screen is special case as the subtitle include directly
      // the percentage  and will be placed in the message placeholder.
      if (screen_id === 'display-size') {
        return this.i18nDynamic(
            locale, 'choobeDisplaySizeSubtitle', screen_subtitle);
      }
      return this.i18nDynamic(locale, screen_subtitle);
    }
    return '';
  }

  isScreenDisabled(is_revisitable, is_completed) {
    return (!is_revisitable) && is_completed;
  }

  isSyncedIconHidden(is_synced, is_completed, is_selected) {
    return (!is_synced) || (is_selected) || (is_completed);
  }

  isScreenVisited(is_selected, is_completed) {
    return is_completed && !is_selected;
  }

  getScreenID(screen_id) {
    return 'cr-button-' + screen_id;
  }

  getAriaLabelToggleButtons_(
      locale, screen_title, screen_subtitle, screen_is_synced,
      screen_is_completed, screen_id, screen_is_selected) {
    var ariaLabel = this.i18nDynamic(locale, screen_title);
    if (screen_subtitle) {
      if (screen_id === 'display-size') {
        ariaLabel = ariaLabel + '.' + screen_subtitle;
      } else {
        ariaLabel = ariaLabel + '.' + this.i18nDynamic(locale, screen_subtitle);
      }
    }
    if (!screen_is_selected && screen_is_completed) {
      ariaLabel =
          ariaLabel + '.' + this.i18nDynamic(locale, 'choobeVisitedTile');
    }
    if (!screen_is_selected && !screen_is_completed && screen_is_synced) {
      ariaLabel =
          ariaLabel + '.' + this.i18nDynamic(locale, 'choobeSyncedTile');
    }
    return ariaLabel;
  }
}

customElements.define(OobeScreensList.is, OobeScreensList);
