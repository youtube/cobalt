// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */
import {constants} from '../../common/constants.js';
import {BrailleKeyEvent} from '../common/braille/braille_key_types.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {TtsSpeechProperties} from '../common/tts_types.js';

export class ChromeVoxState {
  /** @return {!Promise} */
  static ready() {
    return ChromeVoxState.readyPromise_;
  }

  /** Can be overridden to initialize values and state when first created. */
  init() {}

  /** @return {boolean} */
  get isReadingContinuously() {
    return false;
  }

  /** @return {boolean} */
  get talkBackEnabled() {
    return false;
  }

  /**
   * @param {boolean} newValue
   */
  set isReadingContinuously(newValue) {}

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent(evt, content) {}
}

/** @type {ChromeVoxState} */
ChromeVoxState.instance;

/** @type {!Object<string, constants.Point>} */
ChromeVoxState.position = {};

/** @protected {function()} */
ChromeVoxState.resolveReadyPromise_;
/** @private {!Promise} */
ChromeVoxState.readyPromise_ =
    new Promise(resolve => ChromeVoxState.resolveReadyPromise_ = resolve);
