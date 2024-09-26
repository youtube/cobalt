// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a common button control, bound to command.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';
import {getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {decorate} from '../../../common/js/ui.js';
import {Command} from './command.js';

/**
 * Creates a new button element.
 * @extends {HTMLButtonElement}
 */
export class CommandButton {
  constructor() {
    /**
     * Associated command.
     * @private {Command}
     */
    this.command_ = null;
  }

  /**
   * Decorates the given element.
   * @param {!Element} element Item to be decorated.
   * @return {!CommandButton} Decorated item.
   */
  static decorate(element) {
    // Add the CommandButton methods to the element we're
    // decorating, leaving it's prototype chain intact.
    Object.getOwnPropertyNames(CommandButton.prototype).forEach(name => {
      if (name !== 'constructor') {
        element[name] = CommandButton.prototype[name];
      }
    });
    element = /** @type {!CommandButton} */ (element);
    element.decorate();
    return element;
  }

  /**
   * Initializes the menu item.
   */
  decorate() {
    let commandId;
    if ((commandId = this.getAttribute('command'))) {
      this.setCommand(commandId);
    }

    this.addEventListener('click', this.handleClick_.bind(this));
  }

  /**
   * Returns associated command.
   * @return {Command} associated command.
   */
  getCommand() {
    return this.command_;
  }

  /**
   * Associates command with this button.
   * @param {string|Command} command Command id, or command object to
   * associate with this button.
   */
  setCommand(command) {
    if (this.command_) {
      this.command_.removeEventListener(
          'labelChange',
          /** @type {EventListener} */ (this));
      this.command_.removeEventListener(
          'disabledChange',
          /** @type {EventListener} */ (this));
      this.command_.removeEventListener(
          'hiddenChange',
          /** @type {EventListener} */ (this));
    }

    if (typeof command == 'string') {
      assert(command[0] == '#');
      command = /** @type {!Command} */
          (this.ownerDocument.body.querySelector(command));
      decorate(command, Command);
    }

    this.command_ = command;
    if (command) {
      if (command.id) {
        this.setAttribute('command', '#' + command.id);
      }

      this.setLabel(command.label);
      this.disabled = command.disabled;
      this.hidden = command.hidden;

      this.command_.addEventListener(
          'labelChange',
          /** @type {EventListener} */ (this));
      this.command_.addEventListener(
          'disabledChange',
          /** @type {EventListener} */ (this));
      this.command_.addEventListener(
          'hiddenChange',
          /** @type {EventListener} */ (this));
    }
  }

  /**
   * Returns button label
   * @return {string} Button label.
   */
  getLabel() {
    return this.command_ ? this.command_.label : '';
  }

  /**
   * Sets button label.
   * @param {string} label New button label.
   */
  setLabel(label) {
    // Swap the textContent with current label only when this button doesn't
    // have any elements as children.
    //
    // TODO(fukino): If a user customize the button content, it becomes the
    // user's responsibility to update the content on command label's change.
    // Updating the label in customized button content should be done
    // automatically by specifying an element which should be synced with the
    // command label using class name or polymer's template binding.
    if (!this.firstElementChild) {
      this.textContent = label;
    }
  }

  /**
   * Handles click event and dispatches associated command.
   * @param {Event} e The mouseup event object.
   * @private
   */
  handleClick_(e) {
    if (!this.disabled && this.command_) {
      this.command_.execute(this);
    }
  }

  /**
   * Handles changes to the associated command.
   * @param {Event} e The event object.
   */
  handleEvent(e) {
    switch (e.type) {
      case 'disabledChange':
        this.disabled = this.command_.disabled;
        break;
      case 'hiddenChange':
        this.hidden = this.command_.hidden;
        break;
      case 'labelChange':
        this.setLabel(this.command_.label);
        break;
    }
  }
}

/**
 * Whether the button is disabled or not.
 * @type {boolean}
 */
CommandButton.prototype.disabled;

/**
 * Whether the button is hidden or not.
 * @type {boolean}
 */
CommandButton.prototype.hidden;

CommandButton.prototype.__proto__ = HTMLButtonElement.prototype;
