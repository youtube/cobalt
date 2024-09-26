// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../components/common_styles/oobe_common_styles.css.js';
import './assistant_common_styles.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../components/behaviors/oobe_i18n_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 */
const VoiceMatchEntryBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
class VoiceMatchEntry extends VoiceMatchEntryBase {
  static get is() {
    return 'voice-match-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      active: {
        type: Boolean,
        value: false,
      },

      completed: {
        type: Boolean,
        value: false,
      },
    };
  }
}

customElements.define(VoiceMatchEntry.is, VoiceMatchEntry);
