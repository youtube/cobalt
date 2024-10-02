// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Class that implements a macro to list Dictation commands (by opening a Help
 * Center article)
 */
export class ListCommandsMacro extends Macro {
  constructor() {
    super(MacroName.LIST_COMMANDS);
  }

  /** @override */
  checkContext() {
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  run() {
    // Note that this will open a new tab, ending the current Dictation session
    // by changing the input focus.
    globalThis.open(
        'https://support.google.com/chromebook?p=text_dictation_m100',
        '_blank');
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
