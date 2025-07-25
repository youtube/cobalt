// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translates text to braille, optionally with some parts
 * uncontracted.
 */
import {Spannable} from '../../common/spannable.js';

import {LibLouis} from './liblouis.js';
import {BrailleTextStyleSpan, ExtraCellsSpan, ValueSelectionSpan, ValueSpan} from './spans.js';

/**
 * A wrapper around one or two braille translators that uses contracted
 * braille or not based on the selection start- and end-points (if any) in the
 * translated text.  If only one translator is provided, then that translator
 * is used for all text regardless of selection.  If two translators
 * are provided, then the uncontracted translator is used for some text
 * around the selection end-points and the contracted translator is used
 * for all other text.  When determining what text to use uncontracted
 * translation for around a position, a region surrounding that position
 * containing either only whitespace characters or only non-whitespace
 * characters is used.
 */
export class ExpandingBrailleTranslator {
  /**
   * @param {!LibLouis.Translator} defaultTranslator The translator for all
   *     text when the uncontracted translator is not used.
   * @param {LibLouis.Translator=} opt_uncontractedTranslator
   *     Translator to use for uncontracted braille translation.
   */
  constructor(defaultTranslator, opt_uncontractedTranslator) {
    /** @private {!LibLouis.Translator} */
    this.defaultTranslator_ = defaultTranslator;
    /** @private {LibLouis.Translator} */
    this.uncontractedTranslator_ = opt_uncontractedTranslator || null;
  }

  /**
   * Translates text to braille using the translator(s) provided to the
   * constructor.  See {@code LibLouis.Translator} for further details.
   * @param {!Spannable} text Text to translate.
   * @param {ExpandingBrailleTranslator.ExpansionType} expansionType
   *     Indicates how the text marked by a value span, if any, is expanded.
   * @param {function(!ArrayBuffer, !Array<number>, !Array<number>)}
   *     callback Called when the translation is done.  It takes resulting
   *         braille cells and positional mappings as parameters.
   */
  translate(text, expansionType, callback) {
    const expandRanges = this.findExpandRanges_(text, expansionType);
    const extraCellsSpans = text.getSpansInstanceOf(ExtraCellsSpan)
                                .filter(span => span.cells.byteLength > 0);
    const extraCellsPositions =
        extraCellsSpans.map(span => text.getSpanStart(span));
    const formTypeMap = new Array(text.length).fill(0);
    text.getSpansInstanceOf(BrailleTextStyleSpan).forEach(span => {
      const start = text.getSpanStart(span);
      const end = text.getSpanEnd(span);
      for (let i = start; i < end; i++) {
        formTypeMap[i] |= span.formType;
      }
    });

    if (expandRanges.length === 0 && extraCellsSpans.length === 0) {
      this.defaultTranslator_.translate(
          text.toString(), formTypeMap,
          ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
              text.length, callback));
      return;
    }

    const chunks = [];
    function maybeAddChunkToTranslate(translator, start, end) {
      if (start < end) {
        chunks.push({translator, start, end});
      }
    }
    function addExtraCellsChunk(pos, cells) {
      const chunk = {
        translator: null,
        start: pos,
        end: pos,
        cells,
        textToBraille: [],
        brailleToText: new Array(cells.byteLength),
      };
      for (let i = 0; i < cells.byteLength; ++i) {
        chunk.brailleToText[i] = 0;
      }
      chunks.push(chunk);
    }
    function addChunk(translator, start, end) {
      while (extraCellsSpans.length > 0 && extraCellsPositions[0] <= end) {
        maybeAddChunkToTranslate(translator, start, extraCellsPositions[0]);
        start = extraCellsPositions.shift();
        addExtraCellsChunk(start, extraCellsSpans.shift().cells);
      }
      maybeAddChunkToTranslate(translator, start, end);
    }
    let lastEnd = 0;
    for (let i = 0; i < expandRanges.length; ++i) {
      const range = expandRanges[i];
      if (lastEnd < range.start) {
        addChunk(this.defaultTranslator_, lastEnd, range.start);
      }
      addChunk(this.uncontractedTranslator_, range.start, range.end);
      lastEnd = range.end;
    }
    addChunk(this.defaultTranslator_, lastEnd, text.length);

    const chunksToTranslate = chunks.filter(chunk => chunk.translator);
    let numPendingCallbacks = chunksToTranslate.length;

    function chunkTranslated(chunk, cells, textToBraille, brailleToText) {
      chunk.cells = cells;
      chunk.textToBraille = textToBraille;
      chunk.brailleToText = brailleToText;
      if (--numPendingCallbacks <= 0) {
        finish();
      }
    }

    function finish() {
      const totalCells =
          chunks.reduce((accum, chunk) => accum + chunk.cells.byteLength, 0);
      const cells = new Uint8Array(totalCells);
      let cellPos = 0;
      const textToBraille = [];
      const brailleToText = [];
      function appendAdjusted(array, toAppend, adjustment) {
        array.push.apply(array, toAppend.map(elem => adjustment + elem));
      }
      for (let i = 0, chunk; chunk = chunks[i]; ++i) {
        cells.set(new Uint8Array(chunk.cells), cellPos);
        appendAdjusted(textToBraille, chunk.textToBraille, cellPos);
        appendAdjusted(brailleToText, chunk.brailleToText, chunk.start);
        cellPos += chunk.cells.byteLength;
      }
      callback(cells.buffer, textToBraille, brailleToText);
    }

    if (chunksToTranslate.length > 0) {
      chunksToTranslate.forEach(chunk => {
        chunk.translator.translate(
            text.toString().substring(chunk.start, chunk.end),
            formTypeMap.slice(chunk.start, chunk.end),
            ExpandingBrailleTranslator.nullParamsToEmptyAdapter_(
                chunk.end - chunk.start,
                (cells, textToBraille, brailleToText) => chunkTranslated(
                    chunk, cells, textToBraille, brailleToText)));
      });
    } else {
      finish();
    }
  }

  /**
   * Expands a position to a range that covers the consecutive range of
   * either whitespace or non whitespace characters around it.
   * @param {string} str Text to look in.
   * @param {number} pos Position to start looking at.
   * @param {number} start Minimum value for the start position of the returned
   *     range.
   * @param {number} end Maximum value for the end position of the returned
   *     range.
   * @return {!ExpandingBrailleTranslator.Range_} The claculated range.
   * @private
   */
  static rangeForPosition_(str, pos, start, end) {
    if (start < 0 || end > str.length) {
      throw RangeError(
          'End-points out of range looking for braille expansion range');
    }
    if (pos < start || pos >= end) {
      throw RangeError(
          'Position out of range looking for braille expansion range');
    }
    // Find the last chunk of either whitespace or non-whitespace before and
    // including pos.
    start = str.substring(start, pos + 1).search(/(\s+|\S+)$/) + start;
    // Find the characters to include after pos, starting at pos so that
    // they are the same kind (either whitespace or not) as the
    // characters starting at start.
    end = pos + /^(\s+|\S+)/.exec(str.substring(pos, end))[0].length;
    return {start, end};
  }

  /**
   * Finds the ranges in which contracted braille should not be used.
   * @param {!Spannable} text Text to find expansion ranges in.
   * @param {ExpandingBrailleTranslator.ExpansionType} expansionType
   *     Indicates how the text marked up as the value is expanded.
   * @return {!Array<ExpandingBrailleTranslator.Range_>} The calculated
   *     ranges.
   * @private
   */
  findExpandRanges_(text, expansionType) {
    const result = [];
    if (this.uncontractedTranslator_ &&
        expansionType !== ExpandingBrailleTranslator.ExpansionType.NONE) {
      const value = text.getSpanInstanceOf(ValueSpan);
      if (value) {
        const valueStart = text.getSpanStart(value);
        const valueEnd = text.getSpanEnd(value);
        switch (expansionType) {
          case ExpandingBrailleTranslator.ExpansionType.SELECTION:
            this.addRangesForSelection_(text, valueStart, valueEnd, result);
            break;
          case ExpandingBrailleTranslator.ExpansionType.ALL:
            result.push({start: valueStart, end: valueEnd});
            break;
        }
      }
    }

    return result;
  }

  /**
   * Finds ranges to expand around selection end points inside the value of
   * a string.  If any ranges are found, adds them to {@code outRanges}.
   * @param {Spannable} text Text to find ranges in.
   * @param {number} valueStart Start of the value in {@code text}.
   * @param {number} valueEnd End of the value in {@code text}.
   * @param {Array<ExpandingBrailleTranslator.Range_>} outRanges
   *     Destination for the expansion ranges.  Untouched if no ranges
   *     are found.  Note that ranges may be coalesced.
   * @private
   */
  addRangesForSelection_(text, valueStart, valueEnd, outRanges) {
    const selection = text.getSpanInstanceOf(ValueSelectionSpan);
    if (!selection) {
      return;
    }
    const selectionStart = text.getSpanStart(selection);
    const selectionEnd = text.getSpanEnd(selection);
    if (selectionStart < valueStart || selectionEnd > valueEnd) {
      return;
    }
    const expandPositions = [];
    if (selectionStart === valueEnd) {
      if (selectionStart > valueStart) {
        expandPositions.push(selectionStart - 1);
      }
    } else {
      if (selectionStart === selectionEnd && selectionStart > valueStart) {
        expandPositions.push(selectionStart - 1);
      }
      expandPositions.push(selectionStart);
      // Include the selection end if the length of the selection is
      // greater than one (otherwise this position would be redundant).
      if (selectionEnd > selectionStart + 1) {
        // Look at the last actual character of the selection, not the
        // character at the (exclusive) end position.
        expandPositions.push(selectionEnd - 1);
      }
    }

    let lastRange = outRanges[outRanges.length - 1] || null;
    for (let i = 0; i < expandPositions.length; ++i) {
      const range = ExpandingBrailleTranslator.rangeForPosition_(
          text.toString(), expandPositions[i], valueStart, valueEnd);
      if (lastRange && lastRange.end >= range.start) {
        lastRange.end = range.end;
      } else {
        outRanges.push(range);
        lastRange = range;
      }
    }
  }

  /**
   * Adapts {@code callback} to accept null arguments and treat them as if the
   * translation result is empty.
   * @param {number} inputLength Length of the input to the translation.
   *     Used for populating {@code textToBraille} if null.
   * @param {function(!ArrayBuffer, !Array<number>, !Array<number>)} callback
   *     The callback to adapt.
   * @return {function(ArrayBuffer, Array<number>, Array<number>)}
   *     An adapted version of the callback.
   * @private
   */
  static nullParamsToEmptyAdapter_(inputLength, callback) {
    return function(cells, textToBraille, brailleToText) {
      if (!textToBraille) {
        textToBraille = new Array(inputLength);
        for (let i = 0; i < inputLength; ++i) {
          textToBraille[i] = 0;
        }
      }
      callback(cells || new ArrayBuffer(0), textToBraille, brailleToText || []);
    };
  }
}


/**
 * What expansion to apply to the part of the translated string marked by the
 * {@code ValueSpan} spannable.
 * @enum {number}
 */
ExpandingBrailleTranslator.ExpansionType = {
  /**
   * Use the default translator all of the value, regardless of any selection.
   * This is typically used when the user is in the middle of typing and the
   * typing started outside of a word.
   */
  NONE: 0,
  /**
   * Expand text around the selection end-points if any.  If the selection is
   * a cursor, expand the text that occupies the positions right before and
   * after the cursor.  This is typically used when the user hasn't started
   * typing contracted braille or when editing inside a word.
   */
  SELECTION: 1,
  /**
   * Expand all text covered by the value span.  this is typically used when
   * the user is editing a text field where it doesn't make sense to use
   * contracted braille (such as a url or email address).
   */
  ALL: 2,
};


/**
 * A character range with inclusive start and exclusive end positions.
 * @typedef {{start: number, end: number}}
 * @private
 */
ExpandingBrailleTranslator.Range_;
