// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class ContextualCueingInternalsAppElement extends CrLitElement {
  static get is() {
    return 'contextual-cueing-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

customElements.define(
    ContextualCueingInternalsAppElement.is,
    ContextualCueingInternalsAppElement);

declare global {
  interface HTMLElementTagNameMap {
    'contextual-cueing-internals-app': ContextualCueingInternalsAppElement;
  }
}
