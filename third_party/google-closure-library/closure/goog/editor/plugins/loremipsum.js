/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A plugin that fills the field with lorem ipsum text when it's
 * empty and does not have the focus. Applies to both editable and uneditable
 * fields.
 */

goog.provide('goog.editor.plugins.LoremIpsum');

goog.require('goog.asserts');
goog.require('goog.dom');
goog.require('goog.editor.Command');
goog.require('goog.editor.Field');
goog.require('goog.editor.Plugin');
goog.require('goog.editor.node');
goog.require('goog.functions');
goog.require('goog.html.SafeHtml');
goog.require('goog.userAgent');



/**
 * A plugin that manages lorem ipsum state of editable fields.
 * @param {string} message The lorem ipsum message.
 * @constructor
 * @extends {goog.editor.Plugin}
 * @final
 */
goog.editor.plugins.LoremIpsum = function(message) {
  'use strict';
  goog.editor.Plugin.call(this);

  /**
   * The lorem ipsum message.
   * @type {string}
   * @private
   */
  this.message_ = message;
};
goog.inherits(goog.editor.plugins.LoremIpsum, goog.editor.Plugin);


/** @override */
goog.editor.plugins.LoremIpsum.prototype.getTrogClassId =
    goog.functions.constant('LoremIpsum');


/** @override */
goog.editor.plugins.LoremIpsum.prototype.activeOnUneditableFields =
    goog.functions.TRUE;


/**
 * Whether the field is currently filled with lorem ipsum text.
 * @type {boolean}
 * @private
 */
goog.editor.plugins.LoremIpsum.prototype.usingLorem_ = false;


/**
 * Handles queryCommandValue.
 * @param {string} command The command to query.
 * @return {boolean} The result.
 * @override
 */
goog.editor.plugins.LoremIpsum.prototype.queryCommandValue = function(command) {
  'use strict';
  return command == goog.editor.Command.USING_LOREM && this.usingLorem_;
};


/**
 * Handles execCommand.
 * @param {string} command The command to execute.
 *     Should be CLEAR_LOREM or UPDATE_LOREM.
 * @param {*=} opt_placeCursor Whether to place the cursor in the field
 *     after clearing lorem. Should be a boolean.
 * @override
 */
goog.editor.plugins.LoremIpsum.prototype.execCommand = function(
    command, opt_placeCursor) {
  'use strict';
  if (command == goog.editor.Command.CLEAR_LOREM) {
    this.clearLorem_(!!opt_placeCursor);
  } else if (command == goog.editor.Command.UPDATE_LOREM) {
    this.updateLorem_();
  }
};


/** @override */
goog.editor.plugins.LoremIpsum.prototype.isSupportedCommand = function(
    command) {
  'use strict';
  return command == goog.editor.Command.CLEAR_LOREM ||
      command == goog.editor.Command.UPDATE_LOREM ||
      command == goog.editor.Command.USING_LOREM;
};


/**
 * Set the lorem ipsum text in a goog.editor.Field if needed.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.editor.plugins.LoremIpsum.prototype.updateLorem_ = function() {
  'use strict';
  // Try to apply lorem ipsum if:
  // 1) We have lorem ipsum text
  // 2) There's not a dialog open, as that screws
  //    with the dialog's ability to properly restore the selection
  //    on dialog close (since the DOM nodes would get clobbered in FF)
  // 3) We're not using lorem already
  // 4) The field is not currently active (doesn't have focus).
  var fieldObj = this.getFieldObject();
  if (!this.usingLorem_ && !fieldObj.inModalMode() &&
      goog.editor.Field.getActiveFieldId() != fieldObj.id) {
    var field = fieldObj.getElement();
    if (!field) {
      // Fallback on the original element. This is needed by
      // fields managed by click-to-edit.
      field = fieldObj.getOriginalElement();
    }

    goog.asserts.assert(field);
    if (goog.editor.node.isEmpty(field)) {
      this.usingLorem_ = true;

      // Save the old font style so it can be restored when we
      // clear the lorem ipsum style.
      this.oldFontStyle_ = field.style.fontStyle;
      field.style.fontStyle = 'italic';
      fieldObj.setSafeHtml(
          true, goog.html.SafeHtml.htmlEscapePreservingNewlines(this.message_),
          true);
    }
  }
};


/**
 * Clear an EditableField's lorem ipsum and put in initial text if needed.
 *
 * If using click-to-edit mode (where Trogedit manages whether the field
 * is editable), this works for both editable and uneditable fields.
 *
 * TODO(user): Is this really necessary? See TODO below.
 * @param {boolean=} opt_placeCursor Whether to place the cursor in the field
 *     after clearing lorem.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.editor.plugins.LoremIpsum.prototype.clearLorem_ = function(
    opt_placeCursor) {
  'use strict';
  // Don't mess with lorem state when a dialog is open as that screws
  // with the dialog's ability to properly restore the selection
  // on dialog close (since the DOM nodes would get clobbered)
  var fieldObj = this.getFieldObject();
  if (this.usingLorem_ && !fieldObj.inModalMode()) {
    var field = fieldObj.getElement();
    if (!field) {
      // Fallback on the original element. This is needed by
      // fields managed by click-to-edit.
      field = fieldObj.getOriginalElement();
    }

    goog.asserts.assert(field);
    this.usingLorem_ = false;
    field.style.fontStyle = this.oldFontStyle_;
    fieldObj.setSafeHtml(true, null, true);

    // TODO(nicksantos): I'm pretty sure that this is a hack, but talk to
    // Julie about why this is necessary and what to do with it. Really,
    // we need to figure out where it's necessary and remove it where it's
    // not. Safari never places the cursor on its own willpower.
    if (opt_placeCursor && fieldObj.isLoaded()) {
      if (goog.userAgent.WEBKIT) {
        goog.dom.getOwnerDocument(fieldObj.getElement()).body.focus();
        fieldObj.focusAndPlaceCursorAtStart();
      }
    }
  }
};
