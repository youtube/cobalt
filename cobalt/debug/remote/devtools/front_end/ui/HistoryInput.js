// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
<<<<<<< HEAD
UI.HistoryInput = class extends HTMLInputElement {
=======
export default class HistoryInput extends HTMLInputElement {
>>>>>>> bc9bfbcd01448b108b9f2f03cc440b2b92016b02
  constructor() {
    super();
    this._history = [''];
    this._historyPosition = 0;
    this.addEventListener('keydown', this._onKeyDown.bind(this), false);
    this.addEventListener('input', this._onInput.bind(this), false);
<<<<<<< HEAD
  }
  /**
   * @return {!UI.HistoryInput}
   */
  static create() {
    if (!UI.HistoryInput._constructor)
      UI.HistoryInput._constructor = UI.registerCustomElement('input', 'history-input', UI.HistoryInput);

    return /** @type {!UI.HistoryInput} */ (UI.HistoryInput._constructor());
=======
>>>>>>> bc9bfbcd01448b108b9f2f03cc440b2b92016b02
  }
  /**
   * @return {!HistoryInput}
   */
  static create() {
    if (!HistoryInput._constructor) {
      HistoryInput._constructor = UI.registerCustomElement('input', 'history-input', HistoryInput);
    }

    return /** @type {!HistoryInput} */ (HistoryInput._constructor());
  }

  /**
   * @param {!Event} event
   */
  _onInput(event) {
    if (this._history.length === this._historyPosition + 1) {
      this._history[this._history.length - 1] = this.value;
    }
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    if (event.keyCode === UI.KeyboardShortcut.Keys.Up.code) {
      this._historyPosition = Math.max(this._historyPosition - 1, 0);
      this.value = this._history[this._historyPosition];
      this.dispatchEvent(new Event('input', {'bubbles': true, 'cancelable': true}));
      event.consume(true);
    } else if (event.keyCode === UI.KeyboardShortcut.Keys.Down.code) {
      this._historyPosition = Math.min(this._historyPosition + 1, this._history.length - 1);
      this.value = this._history[this._historyPosition];
      this.dispatchEvent(new Event('input', {'bubbles': true, 'cancelable': true}));
      event.consume(true);
    } else if (event.keyCode === UI.KeyboardShortcut.Keys.Enter.code) {
      this._saveToHistory();
    }
  }

  _saveToHistory() {
    if (this._history.length > 1 && this._history[this._history.length - 2] === this.value) {
      return;
    }
    this._history[this._history.length - 1] = this.value;
    this._historyPosition = this._history.length - 1;
    this._history.push('');
  }
}

/* Legacy exported object*/
self.UI = self.UI || {};

/* Legacy exported object*/
UI = UI || {};

/** @constructor */
UI.HistoryInput = HistoryInput;
