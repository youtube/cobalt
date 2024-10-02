// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DragWrapper} from 'chrome://resources/js/drag_wrapper.js';

import {getCardSlider, saveAppPageName} from './new_tab.js';
import {getCurrentlyDraggingTile, setCurrentDropEffect, TilePage} from './tile_page.js';

/**
 * @fileoverview Nav dot
 * This is the class for the navigation controls that appear along the bottom
 * of the NTP.
 */


/**
 * Creates a new navigation dot.
 * @param {TilePage} page The associated TilePage.
 * @param {string} title The title of the navigation dot.
 * @param {boolean} titleIsEditable If true, the title can be changed.
 * @param {boolean} animate If true, animates into existence.
 * @constructor
 * @extends {HTMLLIElement}
 * @implements {DragWrapperDelegate}
 */
export function NavDot(page, title, titleIsEditable, animate) {
  const dot = /** @type {!NavDot} */ (document.createElement('li'));
  dot.__proto__ = NavDot.prototype;
  dot.initialize(page, title, titleIsEditable, animate);

  return dot;
}

NavDot.prototype = {
  __proto__: HTMLLIElement.prototype,

  initialize(page, title, titleIsEditable, animate) {
    this.className = 'dot';
    this.setAttribute('role', 'button');

    this.page_ = page;

    const selectionBar = this.ownerDocument.createElement('div');
    selectionBar.className = 'selection-bar';
    this.appendChild(selectionBar);

    // TODO(estade): should there be some limit to the number of characters?
    this.input_ = this.ownerDocument.createElement('input');
    this.input_.setAttribute('spellcheck', false);
    this.input_.value = title;
    // Take the input out of the tab-traversal focus order.
    this.input_.disabled = true;
    this.appendChild(this.input_);

    this.displayTitle = title;
    this.titleIsEditable_ = titleIsEditable;

    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('click', this.onClick_);
    this.addEventListener('dblclick', this.onDoubleClick_);
    this.dragWrapper_ = new DragWrapper(this, this);
    this.addEventListener('transitionend', this.onTransitionEnd_);

    this.input_.addEventListener('blur', this.onInputBlur_.bind(this));
    this.input_.addEventListener(
        'mousedown', this.onInputMouseDown_.bind(this));
    this.input_.addEventListener('keydown', this.onInputKeyDown_.bind(this));

    if (animate) {
      this.classList.add('small');
      const self = this;
      window.setTimeout(function() {
        self.classList.remove('small');
      }, 0);
    }
  },

  /**
   * @return {TilePage} The associated TilePage.
   */
  get page() {
    return this.page_;
  },

  /**
   * Sets/gets the display title.
   * @type {string} title The display name for this nav dot.
   */
  get displayTitle() {
    return this.title;
  },
  set displayTitle(title) {
    this.title = this.input_.value = title;
  },

  /**
   * Removes the dot from the page. If |opt_animate| is truthy, we first
   * transition the element to 0 width.
   * @param {boolean=} opt_animate Whether to animate the removal or not.
   */
  remove(opt_animate) {
    if (opt_animate) {
      this.classList.add('small');
    } else {
      this.parentNode.removeChild(this);
    }
  },

  /**
   * Navigates the card slider to the page for this dot.
   */
  switchToPage() {
    getCardSlider().selectCardByValue(this.page_, true);
  },

  /**
   * Handler for keydown event on the dot.
   * @param {Event} e The KeyboardEvent.
   */
  onKeyDown_(e) {
    if (e.key === 'Enter') {
      this.onClick_(e);
      e.stopPropagation();
    }
  },

  /**
   * Clicking causes the associated page to show.
   * @param {Event} e The click event.
   * @private
   */
  onClick_(e) {
    this.switchToPage();
    // The explicit focus call is necessary because of overriding the default
    // handling in onInputMouseDown_.
    if (this.ownerDocument.activeElement !== this.input_) {
      this.focus();
    }

    e.stopPropagation();
  },

  /**
   * Double clicks allow the user to edit the page title.
   * @param {Event} e The click event.
   * @private
   */
  onDoubleClick_(e) {
    if (this.titleIsEditable_) {
      this.input_.disabled = false;
      this.input_.focus();
      this.input_.select();
    }
  },

  /**
   * Prevent mouse down on the input from selecting it.
   * @param {Event} e The click event.
   * @private
   */
  onInputMouseDown_(e) {
    if (this.ownerDocument.activeElement !== this.input_) {
      e.preventDefault();
    }
  },

  /**
   * Handle keypresses on the input.
   * @param {Event} e The click event.
   * @private
   */
  onInputKeyDown_(e) {
    switch (e.key) {
      case 'Escape':  // Escape cancels edits.
        this.input_.value = this.displayTitle;
      case 'Enter':  // Fall through.
        this.input_.blur();
        break;
    }
  },

  /**
   * When the input blurs, commit the edited changes.
   * @param {Event} e The blur event.
   * @private
   */
  onInputBlur_(e) {
    window.getSelection().removeAllRanges();
    this.displayTitle = this.input_.value;
    saveAppPageName(this.page_, this.displayTitle);
    this.input_.disabled = true;
  },

  shouldAcceptDrag(e) {
    return this.page_.shouldAcceptDrag(e);
  },

  /** @override */
  doDragEnter(e) {
    const self = this;
    function navPageClearTimeout() {
      self.switchToPage();
      self.dragNavTimeout = null;
    }
    this.dragNavTimeout = window.setTimeout(navPageClearTimeout, 500);

    this.doDragOver(e);
  },

  /** @override */
  doDragOver(e) {
    // Prevent default handling so the <input> won't act as a drag target.
    e.preventDefault();

    if (!this.dragWrapper_.isCurrentDragTarget) {
      setCurrentDropEffect(e.dataTransfer, 'none');
    } else {
      this.page_.setDropEffect(e.dataTransfer);
    }
  },

  /** @override */
  doDrop(e) {
    e.stopPropagation();
    const tile = getCurrentlyDraggingTile();
    if (tile && tile.tilePage !== this.page_) {
      this.page_.appendDraggingTile();
    }
    // TODO(estade): handle non-tile drags.

    this.cancelDelayedSwitch_();
  },

  /** @override */
  doDragLeave(e) {
    this.cancelDelayedSwitch_();
  },

  /**
   * Cancels the timer for page switching.
   * @private
   */
  cancelDelayedSwitch_() {
    if (this.dragNavTimeout) {
      window.clearTimeout(this.dragNavTimeout);
      this.dragNavTimeout = null;
    }
  },

  /**
   * A transition has ended.
   * @param {Event} e The transition end event.
   * @private
   */
  onTransitionEnd_(e) {
    if (e.propertyName === 'max-width' && this.classList.contains('small')) {
      this.parentNode.removeChild(this);
    }
  },
};
