// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';
import {Mode} from './switch_access_constants.js';

/**
 * Class to handle auto-scan behavior.
 */
export class AutoScanManager {
  /** @private */
  constructor() {
    /**
     * Auto-scan interval ID.
     * @private {number|undefined}
     */
    this.intervalID_;

    /**
     * Length of the auto-scan interval for most contexts, in milliseconds.
     * @private {number}
     */
    this.primaryScanTime_ = AutoScanManager.NOT_INITIALIZED;

    /**
     * Length of auto-scan interval for the on-screen keyboard in milliseconds.
     * @private {number}
     */
    this.keyboardScanTime_ = AutoScanManager.NOT_INITIALIZED;

    /**
     * Whether auto-scanning is enabled.
     * @private {boolean}
     */
    this.isEnabled_ = false;

    /**
     * Whether the current node is within the virtual keyboard.
     * @private {boolean}
     */
    this.inKeyboard_ = false;
  }

  // ============== Static Methods ================

  static get instance() {
    if (!AutoScanManager.instance_) {
      AutoScanManager.instance_ = new AutoScanManager();
    }
    return AutoScanManager.instance_;
  }

  /**
   * Restart auto-scan under the current settings if it is currently running.
   */
  static restartIfRunning() {
    if (AutoScanManager.instance.isRunning_()) {
      AutoScanManager.instance.stop_();
      AutoScanManager.instance.start_();
    }
  }

  /**
   * Stop auto-scan if it is currently running. Then, if |enabled| is true,
   * turn on auto-scan. Otherwise leave it off.
   *
   * @param {boolean} enabled
   */
  static setEnabled(enabled) {
    if (AutoScanManager.instance.isRunning_()) {
      AutoScanManager.instance.stop_();
    }
    AutoScanManager.instance.isEnabled_ = enabled;
    if (enabled) {
      AutoScanManager.instance.start_();
    }
  }

  /**
   * Sets whether the keyboard scan time is used.
   * @param {boolean} inKeyboard
   */
  static setInKeyboard(inKeyboard) {
    AutoScanManager.instance.inKeyboard_ = inKeyboard;
  }

  /**
   * Update this.keyboardScanTime_ to |scanTime|.
   *
   * @param {number} scanTime Auto-scan interval time in milliseconds.
   */
  static setKeyboardScanTime(scanTime) {
    AutoScanManager.instance.keyboardScanTime_ = scanTime;
    if (AutoScanManager.instance.inKeyboard_) {
      AutoScanManager.restartIfRunning();
    }
  }

  /**
   * Update this.primaryScanTime_ to |scanTime|. Then, if auto-scan is currently
   * running, restart it.
   * @param {number} scanTime Auto-scan interval time in milliseconds.
   */
  static setPrimaryScanTime(scanTime) {
    AutoScanManager.instance.primaryScanTime_ = scanTime;
    AutoScanManager.restartIfRunning();
  }

  // ============== Private Methods ================

  /**
   * Return true if auto-scan is currently running. Otherwise return false.
   * @return {boolean}
   * @private
   */
  isRunning_() {
    return this.isEnabled_;
  }

  /**
   * Set the window to move to the next node at an interval in milliseconds
   * depending on where the user is navigating. Currently,
   * this.keyboardScanTime_ is used as the interval if the user is
   * navigating in the virtual keyboard, and this.primaryScanTime_ is used
   * otherwise. Does not do anything if AutoScanManager is already scanning.
   *
   * @private
   */
  start_() {
    if (this.primaryScanTime_ === AutoScanManager.NOT_INITIALIZED ||
        this.intervalID_ || SwitchAccess.mode === Mode.POINT_SCAN) {
      return;
    }

    let currentScanTime = this.primaryScanTime_;

    if (SwitchAccess.improvedTextInputEnabled() && this.inKeyboard_ &&
        this.keyboardScanTime_ !== AutoScanManager.NOT_INITIALIZED) {
      currentScanTime = this.keyboardScanTime_;
    }

    this.intervalID_ = setInterval(() => {
      if (SwitchAccess.mode === Mode.POINT_SCAN) {
        AutoScanManager.instance.stop_();
        return;
      }
      Navigator.byItem.moveForward();
    }, currentScanTime);
  }

  /**
   * Stop the window from moving to the next node at a fixed interval.
   * @private
   */
  stop_() {
    clearInterval(this.intervalID_);
    this.intervalID_ = undefined;
  }
}

/**
 * Sentinel value that indicates an uninitialized scan time.
 * @type {number}
 */
AutoScanManager.NOT_INITIALIZED = -1;
