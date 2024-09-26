// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple container object for the brailling of a
 * navigation from one object to another.
 *
 */

/**
 * A class capturing the braille for navigation from one object to
 * another.
 */

import {Spannable} from '../spannable.js';

export class NavBraille {
  /**
   * @param {{text: (undefined|string|!Spannable),
   *          startIndex: (undefined|number),
   *          endIndex: (undefined|number)}} kwargs The arguments for braille.
   *  text The text of the object itself, including text from
   *     titles, labels, etc.
   *  startIndex The beginning of a selection within text.
   *  endIndex The end of a selection within text.
   */
  constructor(kwargs) {
    /**
     * Text, annotated with DOM nodes.
     * @type {!Spannable}
     */
    this.text = (kwargs.text instanceof Spannable) ? kwargs.text :
                                                     new Spannable(kwargs.text);

    /**
     * Selection start index.
     * @type {number}
     */
    this.startIndex =
        (kwargs.startIndex !== undefined) ? kwargs.startIndex : -1;

    /**
     * Selection end index.
     * @type {number}
     */
    this.endIndex =
        (kwargs.endIndex !== undefined) ? kwargs.endIndex : this.startIndex;
  }

  /**
   * Convenience for creating simple braille output.
   * @param {string|!Spannable} text Text to represent in braille.
   * @return {!NavBraille} Braille output without a cursor.
   */
  static fromText(text) {
    return new NavBraille({text});
  }

  /**
   * Creates a NavBraille from its serialized json form as created
   * by toJson().
   * @param {!Object} json the serialized json object.
   * @return {!NavBraille}
   */
  static fromJson(json) {
    if (typeof json.startIndex !== 'number' ||
        typeof json.endIndex !== 'number') {
      throw 'Invalid start or end index in serialized NavBraille: ' + json;
    }
    return new NavBraille({
      text: Spannable.fromJson(json.spannable),
      startIndex: json.startIndex,
      endIndex: json.endIndex,
    });
  }

  /**
   * @return {boolean} true if this braille description is empty.
   */
  isEmpty() {
    return this.text.length === 0;
  }

  /**
   * @return {string} A string representation of this object.
   */
  toString() {
    return 'NavBraille(text="' + this.text.toString() + '" ' +
        ' startIndex="' + this.startIndex + '" ' +
        ' endIndex="' + this.endIndex + '")';
  }

  /**
   * Returns a plain old data object with the same data.
   * Suitable for JSON encoding.
   *
   * @return {{spannable: Object,
   *           startIndex: number,
   *           endIndex: number}} JSON equivalent.
   */
  toJson() {
    return {
      spannable: this.text.toJson(),
      startIndex: this.startIndex,
      endIndex: this.endIndex,
    };
  }
}
