// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a global object that holds references to the three
 * different output engines.
 */
import {NavBraille} from '../common/braille/nav_braille.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Spannable} from '../common/spannable.js';

import {AbstractEarcons} from './abstract_earcons.js';
import {BrailleInterface} from './braille/braille_interface.js';
import {TtsInterface} from './tts_interface.js';

export const ChromeVox = {
  /** @type {BrailleInterface} */
  braille: null,
  /** @type {AbstractEarcons} */
  earcons: null,
  /** @type {TtsInterface} */
  tts: null,
};

// Braille bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET,
    BridgeConstants.Braille.Action.BACK_TRANSLATE,
    cells => Promise.resolve(ChromeVox.braille?.backTranslate(cells)));

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.PAN_LEFT,
    () => ChromeVox.braille?.panLeft());

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.PAN_RIGHT,
    () => ChromeVox.braille?.panRight());

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.WRITE,
    text =>
        ChromeVox.braille?.write(new NavBraille({text: new Spannable(text)})));

// Earcon bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.Earcons.TARGET,
    BridgeConstants.Earcons.Action.CANCEL_EARCON,
    earconId => ChromeVox.earcons?.cancelEarcon(earconId));

BridgeHelper.registerHandler(
    BridgeConstants.Earcons.TARGET, BridgeConstants.Earcons.Action.PLAY_EARCON,
    earconId => ChromeVox.earcons?.playEarcon(earconId));

// TTS bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.TtsBackground.TARGET,
    BridgeConstants.TtsBackground.Action.SPEAK,
    (text, queueMode, properties) =>
        ChromeVox.tts?.speak(text, queueMode, properties));
