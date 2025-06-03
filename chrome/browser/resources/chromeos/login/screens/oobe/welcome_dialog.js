// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/js/action_link.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/oobe_vars/oobe_shared_vars.css.js';
import '../../components/buttons/oobe_icon_button.js';
import '../../components/hd_iron_icon.js';
import '../../components/quick_start_entry_point.js';

import {assert} from '//resources/ash/common/assert.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LongTouchDetector} from '../../components/long_touch_detector.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const OobeWelcomeDialogBase =
    mixinBehaviors([OobeI18nBehavior, OobeDialogHostBehavior], PolymerElement);

/**
 * @typedef {{
 *   title:  HTMLAnchorElement,
 *   chromeVoxHint:  OobeModalDialog,
 *   welcomeAnimation:  OobeCrLottie,
 * }}
 */
OobeWelcomeDialogBase.$;

/**
 * @polymer
 */
export class OobeWelcomeDialog extends OobeWelcomeDialogBase {
  static get is() {
    return 'oobe-welcome-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Currently selected system language (display name).
       */
      currentLanguage: String,

      /**
       * Controls visibility of "Timezone" button.
       */
      timezoneButtonVisible: Boolean,

      /**
       * Controls displaying of "Enable debugging features" link.
       */
      debuggingLinkVisible: Boolean,

      /**
       * Observer for when this screen is hidden, or shown.
       */
      hidden: {
        type: Boolean,
        observer: 'updateHidden_',
        reflectToAttribute: true,
      },

      isMeet_: {
        type: Boolean,
        value: function() {
          return (
              loadTimeData.valueExists('deviceFlowType') &&
              loadTimeData.getString('deviceFlowType') == 'meet');
        },
        readOnly: true,
      },

      isSimon_: {
        type: Boolean,
        value: function() {
          return (
              loadTimeData.valueExists('isOobeSimonEnabled') &&
              loadTimeData.getBoolean('isOobeSimonEnabled'));
        },
        readOnly: true,
      },

      isDeviceRequisitionConfigurable_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isDeviceRequisitionConfigurable');
        },
        readOnly: true,
      },

      isOobeLoaded_: {
        type: Boolean,
        value: false,
      },

      isQuickStartEnabled: Boolean,
    };
  }

  constructor() {
    super();
    this.currentLanguage = '';
    this.timezoneButtonVisible = false;

    /**
     * @private {LongTouchDetector}
     */
    this.titleLongTouchDetector_ = null;
    /**
     * This is stored ID of currently focused element to restore id on returns
     * to this dialog from Language / Timezone Selection dialogs.
     */
    this.focusedElement_ = null;

    this.isQuickStartEnabled = false;
  }

  ready() {
    super.ready();
    if (loadTimeData.getBoolean('isOobeLazyLoadingEnabled')) {
      // Disable the 'Get Started' & 'Enable Debugging' button until OOBE is
      // fully initialized.
      this.$.getStarted.disabled = true;
      this.$.enableDebuggingButton.disabled = true;
      document.addEventListener(
        'oobe-screens-loaded', this.enableButtonsWhenLoaded.bind(this));
    }
  }

  onBeforeShow() {
    this.setVideoPlay_(true);
  }

  /**
   * Since we prioritize the showing of the the Welcome Screen, it becomes
   * visible before the remaining of the OOBE flow is fully loaded. For this
   * reason, we listen to the |oobe-screens-loaded| signal and enable it.
   */
  enableButtonsWhenLoaded(e) {
    document.removeEventListener(
      'oobe-screens-loaded', this.enableButtonsWhenLoaded.bind(this));
    this.$.getStarted.disabled = false;
    this.$.enableDebuggingButton.disabled = false;
    this.isOobeLoaded_ = true;
  }

  onLanguageClicked_(e) {
    this.focusedElement_ = 'languageSelectionButton';
    this.dispatchEvent(new CustomEvent('language-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  onAccessibilityClicked_() {
    this.focusedElement_ = 'accessibilitySettingsButton';
    this.dispatchEvent(new CustomEvent('accessibility-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  onTimezoneClicked_() {
    this.focusedElement_ = 'timezoneSettingsButton';
    this.dispatchEvent(new CustomEvent('timezone-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  onNextClicked_() {
    this.focusedElement_ = 'getStarted';
    this.dispatchEvent(new CustomEvent(
        'next-button-clicked', {bubbles: true, composed: true}));
  }

  onQuickStartClicked_() {
    assert(this.isQuickStartEnabled);
    this.dispatchEvent(new CustomEvent(
        'quick-start-clicked', {bubbles: true, composed: true}));
  }

  onDebuggingLinkClicked_() {
    this.dispatchEvent(new CustomEvent('enable-debugging-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  /*
   * This is called from titleLongTouchDetector_ when long touch is detected.
   *
   * @private
   */
  onTitleLongTouch_() {
    this.dispatchEvent(new CustomEvent('launch-advanced-options', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @suppress {missingProperties}
   */
  attached() {
    // Allow opening advanced options only if it is a meet device or device
    // requisition is configurable.
    if (this.isMeet_ || this.isDeviceRequisitionConfigurable_) {
      this.titleLongTouchDetector_ = new LongTouchDetector(
          this.$.title, () => void this.onTitleLongTouch_());
    }
    this.$.chromeVoxHint.addEventListener('keydown', (event) => {
      // When the ChromeVox hint dialog is open, allow users to press the
      // space bar to activate ChromeVox. This is intended to help first time
      // users easily activate ChromeVox.
      if (this.$.chromeVoxHint.open && event.key === ' ') {
        this.activateChromeVox_();
        event.preventDefault();
        event.stopPropagation();
      }
    });
    this.focus();
  }

  focus() {
    if (!this.focusedElement_) {
      this.focusedElement_ = 'getStarted';
    }
    const focusedElement = this.$[this.focusedElement_];
    if (focusedElement) {
      focusedElement.focus();
    }
  }

  /*
   * Observer method for changes to the hidden property.
   * This replaces the show() function, in this class.
   */
  updateHidden_(newValue, oldValue) {
    const visible = !newValue;
    if (visible) {
      this.focus();
    }

    this.setVideoPlay_(visible);
  }

  /**
   * Play or pause welcome video.
   * @param {boolean} play - whether play or pause welcome video.
   * @private
   * @suppress {missingProperties}
   */
  setVideoPlay_(play) {
    // Postpone the call until OOBE is loaded, if necessary.
    if (!this.isOobeLoaded_) {
      document.addEventListener(
        'oobe-screens-loaded', () => {
          this.isOobeLoaded_ = true;
          this.setVideoPlay_(play);
        }, { once: true });
      return;
    }

    if (this.$$('#welcomeAnimation')) {
      this.$$('#welcomeAnimation').playing = play;
    }
  }

  /**
   * This function formats message for labels.
   * @param {string} label i18n string ID.
   * @param {string} parameter i18n string parameter.
   * @private
   */
  formatMessage_(label, parameter) {
    return loadTimeData.getStringF(label, parameter);
  }

  // ChromeVox hint section.

  /**
   * Called to show the ChromeVox hint dialog.
   */
  showChromeVoxHint() {
    this.$.chromeVoxHint.showDialog();
    this.setVideoPlay_(false);
  }

  /**
   * Called to close the ChromeVox hint dialog.
   */
  closeChromeVoxHint() {
    this.setVideoPlay_(true);
    this.$.chromeVoxHint.hideDialog();
  }

  /**
   * Called when the 'Continue without ChromeVox' button is clicked.
   * @private
   */
  dismissChromeVoxHint_() {
    this.dispatchEvent(new CustomEvent('chromevox-hint-dismissed', {
      bubbles: true,
      composed: true,
    }));
    this.closeChromeVoxHint();
  }

  /** @private */
  activateChromeVox_() {
    this.closeChromeVoxHint();
    this.dispatchEvent(new CustomEvent('chromevox-hint-accepted', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * Determines if AnimationSlot is needed for specific flow
   */
  showAnimationSlot() {
    return !this.isSimon_;
  }
}

customElements.define(OobeWelcomeDialog.is, OobeWelcomeDialog);
