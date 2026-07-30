// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkTextAnnotationsElement} from './ink_text_annotations.js';

export function getHtml(this: InkTextAnnotationsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" role="list" aria-label="Text Annotations">
  ${this.annotations_.map((annotation, index) => html`
    <div class="placeholder"
        data-index="${index}"
        data-rotations="${this.getPlaceholderRotations_(annotation)}"
        role="listitem"
        aria-label="${annotation.text}"
        tabindex="0"
        @focus="${this.onPlaceholderFocus_}">
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
