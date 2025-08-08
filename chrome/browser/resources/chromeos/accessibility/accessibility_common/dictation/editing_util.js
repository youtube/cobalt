// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocaleInfo} from './locale_info.js';

/**
 * EditingUtil provides utility and helper methods for editing-related
 * operations.
 */
export class EditingUtil {
  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * Returns data needed by inputController.replacePhrase(). Calculates the new
   * caret index and number of characters to be deleted. Only operates on the
   * text to the left of the text caret. If multiple instances of `deletePhrase`
   * are present, this function will operate on the one closest one to the text
   * caret.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @param {string} deletePhrase The phrase to be deleted.
   * @return {?{
   *  newIndex: number,
   *  deleteLength: number,
   * }}
   */
  static getReplacePhraseData(value, caretIndex, deletePhrase) {
    const leftOfCaret = value.substring(0, caretIndex);
    deletePhrase = deletePhrase.trim();

    // Find the right-most occurrence of `deletePhrase`. If we're deleting text,
    // prefer the RegExps that include a leading/trailing space to preserve
    // spacing.
    let re;
    if (LocaleInfo.considerSpaces()) {
      re = EditingUtil.getPhraseRegex_(deletePhrase);
    } else {
      re = EditingUtil.getPhraseRegexNoWordBoundaries_(deletePhrase);
    }
    const reWithLeadingSpace =
        EditingUtil.getPhraseRegexLeadingSpace_(deletePhrase);
    const reWithTrailingSpace =
        EditingUtil.getPhraseRegexTrailingSpace_(deletePhrase);

    const leadingSpaceResult =
        EditingUtil.getIndexFromRegex_(reWithLeadingSpace, leftOfCaret);
    const trailingSpaceResult =
        EditingUtil.getIndexFromRegex_(reWithTrailingSpace, leftOfCaret);
    const noSpacesResult = EditingUtil.getIndexFromRegex_(re, leftOfCaret);

    let newIndex = caretIndex;
    let deleteLength = 0;
    if (leadingSpaceResult !== -1) {
      // Delete one extra character to preserve spacing.
      newIndex = leadingSpaceResult;
      deleteLength = deletePhrase.length + 1;
    } else if (trailingSpaceResult !== -1) {
      // Delete one extra character to preserve spacing.
      newIndex = trailingSpaceResult;
      deleteLength = deletePhrase.length + 1;
    } else if (noSpacesResult !== -1) {
      // Matched with no spacing.
      newIndex = noSpacesResult;
      deleteLength = deletePhrase.length;
    } else {
      // No match.
      return null;
    }

    return {newIndex, deleteLength};
  }

  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * Calculates the new caret index for `inputController.insertBefore`. Only
   * operates on the text to the left of the text caret. If multiple instances
   * of `beforePhrase` are present, this function will operate on the one
   * closest one to the text caret.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @param {string} beforePhrase
   * @return {number}
   */
  static getInsertBeforeIndex(value, caretIndex, beforePhrase) {
    const result =
        EditingUtil.getReplacePhraseData(value, caretIndex, beforePhrase);
    return result ? result.newIndex : -1;
  }

  /**
   * Returns a selection starting at `startPhrase` and ending at `endPhrase`
   * (inclusive). The function operates on the text to the left of the text
   * caret. If multiple instances of `startPhrase` or `endPhrase` are present,
   * the function will use the ones closest to the text caret.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @param {string} startPhrase
   * @param {string} endPhrase
   * @return {{start: number, end: number}|null}
   */
  static selectBetween(value, caretIndex, startPhrase, endPhrase) {
    const leftOfCaret = value.substring(0, caretIndex);
    startPhrase = startPhrase.trim();
    endPhrase = endPhrase.trim();

    let startRe;
    let endRe;
    if (LocaleInfo.considerSpaces()) {
      startRe = EditingUtil.getPhraseRegex_(startPhrase);
      endRe = EditingUtil.getPhraseRegex_(endPhrase);
    } else {
      startRe = EditingUtil.getPhraseRegexNoWordBoundaries_(startPhrase);
      endRe = EditingUtil.getPhraseRegexNoWordBoundaries_(endPhrase);
    }
    const start = leftOfCaret.search(startRe);
    let end = leftOfCaret.search(endRe);
    if (start === -1 || end === -1) {
      return null;
    }

    // Adjust `end` so that we return an inclusive selection.
    end += endPhrase.length;
    if (start > end) {
      return null;
    }

    return {start, end};
  }

  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * Returns the start index of the sentence to the right of the caret.
   * Indices are relative to `value`. Assumes that sentences are separated by
   * punctuation specified in `EditingUtil.END_OF_SENTENCE_REGEX_`. If no next
   * sentence can be found, returns `value.length`.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @return {number}
   */
  static navNextSent(value, caretIndex) {
    const rightOfCaret = value.substring(caretIndex);
    const index = rightOfCaret.search(EditingUtil.END_OF_SENTENCE_REGEX_);
    if (index === -1) {
      return value.length;
    }

    // `index` should be relative to `value`;
    return index + caretIndex + 1;
  }

  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * Returns the start index of the sentence to the left of the caret. Indices
   * are relative to `value`. Assumes that sentences are separated by
   * punctuation specified in `EditingUtil.END_OF_SENTENCE_REGEX_`. If no
   * previous sentence can be found, returns 0.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @return {number}
   */
  static navPrevSent(value, caretIndex) {
    let encounteredText = false;
    if (caretIndex === value.length) {
      --caretIndex;
    }

    while (caretIndex >= 0) {
      const valueAtCaret = value[caretIndex];
      if (encounteredText &&
          EditingUtil.END_OF_SENTENCE_REGEX_.test(valueAtCaret)) {
        // Adjust if there is whitespace immediately to the right of the caret.
        return EditingUtil.BEGINS_WITH_WHITESPACE_REGEX_.test(
                   value[caretIndex + 1]) ?
            caretIndex + 1 :
            caretIndex;
      }

      if (!EditingUtil.BEGINS_WITH_WHITESPACE_REGEX_.test(valueAtCaret) &&
          !EditingUtil.PUNCTUATION_REGEX_.test(valueAtCaret)) {
        encounteredText = true;
      }
      --caretIndex;
    }

    return 0;
  }


  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * This function analyzes the context and adjusts the spacing of `commitText`
   * to maintain proper spacing between text.
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @param {string} commitText
   * @return {string}
   */
  static smartSpacing(value, caretIndex, commitText) {
    // There is currently a bug in SODA (b/213934503) where final speech results
    // do not start with a space. This results in a Dictation bug
    // (crbug.com/1294050), where final speech results are not separated by a
    // space when committed to a text field. This is a temporary workaround
    // until the blocking SODA bug can be fixed. Note, a similar strategy
    // already exists in Dictation::OnSpeechResult().
    if (!value || EditingUtil.BEGINS_WITH_WHITESPACE_REGEX_.test(commitText) ||
        EditingUtil.BEGINS_WITH_PUNCTUATION_REGEX_.test(commitText)) {
      return commitText;
    }

    commitText = commitText.trim();
    const leftOfCaret = value.substring(0, caretIndex);
    const rightOfCaret = value.substring(caretIndex);

    if (leftOfCaret &&
        !EditingUtil.ENDS_WITH_WHITESPACE_REGEX_.test(leftOfCaret)) {
      // If there is no whitespace before the caret index, prepend a space.
      commitText = ' ' + commitText;
    }

    if (rightOfCaret &&
        (!EditingUtil.BEGINS_WITH_WHITESPACE_REGEX_.test(rightOfCaret) ||
         EditingUtil.BEGINS_WITH_PUNCTUATION_REGEX_.test(rightOfCaret))) {
      // If there are no spaces or there is punctuation after the caret index,
      // append a space.
      commitText = commitText + ' ';
    }

    return commitText;
  }

  /**
   * TODO(https://crbug.com/1331351): Add RTL support.
   * This function analyzes the context and adjusts the capitalization of
   * `commitText` as needed. See below for sample input and output: value:
   * 'Hello world.' caretIndex: value.length commitText: 'goodnight world'
   * return value: 'Goodnight world'
   * @param {string} value The current value of the text field.
   * @param {number} caretIndex
   * @param {string} commitText
   * @return {string}
   */
  static smartCapitalization(value, caretIndex, commitText) {
    if (EditingUtil.BEGINS_WITH_PUNCTUATION_REGEX_.test(commitText)) {
      // If `commitText` begins with punctuation, then it's assumed that it's
      // already correctly capitalized.
      return commitText;
    }

    if (!value) {
      return EditingUtil.capitalize_(commitText);
    }

    const leftOfCaret = value.substring(0, caretIndex).trim();
    if (!leftOfCaret) {
      return EditingUtil.capitalize_(commitText);
    }

    return EditingUtil.ENDS_WITH_END_OF_SENTENCE_REGEX_.test(leftOfCaret) ?
        EditingUtil.capitalize_(commitText) :
        EditingUtil.lowercase_(commitText);
  }

  /**
   * Returns a string where the first character is capitalized.
   * @param {string} text
   * @return {string}
   * @private
   */
  static capitalize_(text) {
    return text.charAt(0).toUpperCase() + text.substring(1);
  }

  /**
   * Returns a string where the first character is lowercase.
   * @param {string} text
   * @return {string}
   * @private
   */
  static lowercase_(text) {
    return text.charAt(0).toLowerCase() + text.substring(1);
  }

  /**
   * TODO(akihiroota): Break regex construction into smaller chunks.
   * Returns a RegExp that matches on the right-most occurrence of a phrase.
   * The returned RegExp is case insensitive and requires that `phrase` is
   * separated by word boundaries.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegex_(phrase) {
    return new RegExp(`(\\b${phrase}\\b)(?!.*\\b\\1\\b)`, 'i');
  }

  /**
   * Similar to above, but doesn't include word boundaries. This is useful for
   * languages that don't use spaces e.g. Japanese.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegexNoWordBoundaries_(phrase) {
    return new RegExp(`(${phrase})(?!.*\\1)`, 'i');
  }

  /**
   * Similar to above, but accounts for a leading space.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegexLeadingSpace_(phrase) {
    return new RegExp(`( \\b${phrase}\\b)(?!.*\\b\\1\\b)`, 'i');
  }

  /**
   * Similar to above, but accounts for a trailing space.
   * @param {string} phrase
   * @return {!RegExp}
   * @private
   */
  static getPhraseRegexTrailingSpace_(phrase) {
    return new RegExp(`(\\b${phrase}\\b )(?!.*\\b\\1\\b)`, 'i');
  }

  /**
   * @param {!RegExp} re
   * @param {string} str
   * @return {number}
   * @private
   */
  static getIndexFromRegex_(re, str) {
    const result = re.exec(str);
    return result ? result.index : -1;
  }
}

/**
 * Includes full-width symbols that are commonly used in Japanese.
 * @private {!RegExp}
 * @const
 */
EditingUtil.END_OF_SENTENCE_REGEX_ = /[;!.?。．？！]/;

/**
 * Similar to above, but looks for a match at the end of a string.
 * @private {!RegExp}
 * @const
 */
EditingUtil.ENDS_WITH_END_OF_SENTENCE_REGEX_ = /[;!.?。．？！]$/;

/**
 * @private {!RegExp}
 * @const
 */
EditingUtil.BEGINS_WITH_WHITESPACE_REGEX_ = /^\s/;

/**
 * @private {!RegExp}
 * @const
 */
EditingUtil.ENDS_WITH_WHITESPACE_REGEX_ = /\s$/;

/**
 * @private {!RegExp}
 * @const
 */
EditingUtil.PUNCTUATION_REGEX_ =
    /[-$#"()*;:<>\\\/\{\}\[\]+='~`!@_.,?%。．？！\u2022\u25e6\u25a0]/g;

/**
 * @private {!RegExp}
 * @const
 */
EditingUtil.BEGINS_WITH_PUNCTUATION_REGEX_ =
    /^[-$#"()*;:<>\\\/\{\}\[\]+='~`!@_.,?%。．？！\u2022\u25e6\u25a0]/;
