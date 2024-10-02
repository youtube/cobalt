// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automatic sticky mode toggles. Turns sticky mode off
 * when the current range is over an editable; restores sticky mode when not on
 * an editable.
 */
import {AutomationUtil} from '../../common/automation_util.js';
import {CursorRange} from '../../common/cursors/range.js';
import {EarconId} from '../common/earcon_id.js';
import {SettingsManager} from '../common/settings_manager.js';

import {ChromeVox} from './chromevox.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from './chromevox_range.js';
import {ChromeVoxState} from './chromevox_state.js';
import {ChromeVoxBackground} from './classic_background.js';
import {ChromeVoxPrefs} from './prefs.js';

/** @implements {ChromeVoxRangeObserver} */
export class SmartStickyMode {
  constructor() {
    /** @private {boolean} */
    this.ignoreRangeChanges_ = false;

    /**
     * Tracks whether we (and not the user) turned off sticky mode when over an
     * editable.
     * @private {boolean}
     */
    this.didTurnOffStickyMode_ = false;
    /** @private {chrome.automation.AutomationNode|undefined} */
    this.ignoredNodeSubtree_;

    ChromeVoxRange.addObserver(this);
  }

  static init() {
    if (SmartStickyMode.instance) {
      throw new Error('SmartStickyMode.init() should only be called once');
    }
    SmartStickyMode.instance = new SmartStickyMode();
  }

  /**
   * @param {?CursorRange} newRange
   * @param {boolean=} opt_fromEditing
   * @override
   */
  onCurrentRangeChanged(newRange, opt_fromEditing) {
    if (!newRange || this.ignoreRangeChanges_ ||
        ChromeVoxState.instance.isReadingContinuously || opt_fromEditing ||
        !SettingsManager.get('smartStickyMode')) {
      return;
    }

    const node = newRange.start.node;
    if (this.ignoredNodeSubtree_) {
      const editableOrRelatedEditable =
          this.getEditableOrRelatedEditable_(node);
      if (editableOrRelatedEditable &&
          AutomationUtil.isDescendantOf(
              editableOrRelatedEditable, this.ignoredNodeSubtree_)) {
        return;
      }

      // Clear it when the user leaves the subtree.
      this.ignoredNodeSubtree_ = undefined;
    }

    // Several cases arise which may lead to a sticky mode toggle:
    // The node is either editable itself or a descendant of an editable.
    // The node is a relation target of an editable.
    const shouldTurnOffStickyMode =
        Boolean(this.getEditableOrRelatedEditable_(node));

    // This toggler should not make any changes when the range isn't what we're
    // looking for and we haven't previously tracked any sticky mode state from
    // the user.
    if (!shouldTurnOffStickyMode && !this.didTurnOffStickyMode_) {
      return;
    }

    if (shouldTurnOffStickyMode) {
      if (!ChromeVoxPrefs.isStickyPrefOn) {
        // Sticky mode was already off; do not track the current sticky state
        // since we may have set it ourselves.
        return;
      }

      if (this.didTurnOffStickyMode_) {
        // This should not be possible with |ChromeVoxPrefs.isStickyPrefOn| set
        // to true.
        throw 'Unexpected sticky state value encountered.';
      }

      // Save the sticky state for restoration later.
      this.didTurnOffStickyMode_ = true;
      ChromeVox.earcons.playEarcon(EarconId.SMART_STICKY_MODE_OFF);
      ChromeVoxPrefs.instance.setAndAnnounceStickyPref(false);
    } else if (this.didTurnOffStickyMode_) {
      // Restore the previous sticky mode state.
      ChromeVox.earcons.playEarcon(EarconId.SMART_STICKY_MODE_ON);
      ChromeVoxPrefs.instance.setAndAnnounceStickyPref(true);
      this.didTurnOffStickyMode_ = false;
    }
  }

  /**
   * When called, ignores all changes in the current range when toggling sticky
   * mode without user input.
   */
  startIgnoringRangeChanges() {
    this.ignoreRangeChanges_ = true;
  }

  /**
   * When called, stops ignoring changes in the current range when toggling
   * sticky mode without user input.
   */
  stopIgnoringRangeChanges() {
    this.ignoreRangeChanges_ = false;
  }

  /**
   * Called whenever a user toggles sticky mode. In this case, we need to ensure
   * we reset our internal state appropriately.
   * @param {!CursorRange} range The range when the sticky mode command was
   *     received.
   * @private
   */
  onStickyModeCommand_(range) {
    if (!this.didTurnOffStickyMode_) {
      return;
    }

    this.didTurnOffStickyMode_ = false;

    // Resetting the above isn't quite enough. We now have to track the current
    // range, if it is editable or has an editable relation, to ensure we don't
    // interfere with the user's sticky mode state.
    if (!range || !range.start) {
      return;
    }

    let editable = this.getEditableOrRelatedEditable_(range.start.node);
    while (editable && !editable.nonAtomicTextFieldRoot) {
      if (!editable.parent ||
          editable.parent.state[chrome.automation.StateType.EDITABLE]) {
        // Not all editables from all trees (e.g. Android, views) set the
        // editable root boolean attribute.
        break;
      }
      editable = editable.parent;
    }
    this.ignoredNodeSubtree_ = editable;
  }

  /** Toggles basic stickyMode on or off. */
  toggle() {
    ChromeVoxPrefs.instance.setAndAnnounceStickyPref(
        !ChromeVoxPrefs.isStickyPrefOn);

    if (ChromeVoxRange.current) {
      this.onStickyModeCommand_(ChromeVoxRange.current);
    }
  }


  /**
   * @param {chrome.automation.AutomationNode} node
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  getEditableOrRelatedEditable_(node) {
    if (!node) {
      return null;
    }

    if (node.state[chrome.automation.StateType.EDITABLE]) {
      return node;
    } else if (
        node.parent &&
        node.parent.state[chrome.automation.StateType.EDITABLE]) {
      // This covers inline text boxes (which are not
      // editable themselves, but may have an editable parent).
      return node.parent;
    } else {
      let focus = node;
      let found;
      while (!found && focus) {
        if (focus.activeDescendantFor && focus.activeDescendantFor.length) {
          found = focus.activeDescendantFor.find(
              n => n.state[chrome.automation.StateType.EDITABLE]);
        }

        if (found) {
          return found;
        }

        if (focus.controlledBy && focus.controlledBy.length) {
          found = focus.controlledBy.find(
              n => n.state[chrome.automation.StateType.EDITABLE]);
        }

        if (found) {
          return found;
        }

        focus = focus.parent;
      }
    }

    return null;
  }
}

/** @public {SmartStickyMode} */
SmartStickyMode.instance;
