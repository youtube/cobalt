// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkTextAnnotationsElement} from './ink_text_annotations.js';

export function getHtml(this: InkTextAnnotationsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" role="list" aria-label="$i18n{ink2TextAnnotationsAxLabel}">
  ${this.annotations_.map((annotation, index) => html`
    <div class="placeholder"
        data-index="${index}"
        data-rotations="${this.getPlaceholderRotations_(annotation)}"
        role="listitem"
        aria-label="${annotation.text}"
        tabindex="${this.activeAnnotation_ ? '-1' : '0'}"
        @focus="${this.onPlaceholderFocus_}"
        @click="${this.onPlaceholderClick_}"
        @keydown="${this.onPlaceholderKeydown_}">
    </div>
  `)}
</div>
<ink-text-box id="textBox"
    .viewport="${this.viewport}"
    .annotation="${this.activeAnnotation_}"
    .pageDimensions="${this.activePageDimensions_}"
    @state-changed="${this.onTextBoxStateChanged_}"
    @textbox-focused="${this.onTextboxFocused_}">
</ink-text-box>
  <!--_html_template_end_-->`;
  // clang-format on
}
