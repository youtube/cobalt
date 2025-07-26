// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkColorSelectorElement} from './ink_color_selector.js';

export function getHtml(this: InkColorSelectorElement) {
  return html`
    <div @keydown="${this.onColorKeydown_}">
      ${this.getCurrentBrushColors_().map((item, index) => html`
        <label class="color-item">
          <input type="radio" class="color-chip" data-index="${index}"
              name="${this.getColorName_()}" .value="${item.color}"
              .style="--item-color: ${this.getVisibleColor_(item.color)}"
              aria-label="${this.i18n(item.label)}"
              title="${this.i18n(item.label)}"
              @click="${this.onColorClick_}"
              ?checked="${this.isCurrentColor_(item.color)}">
        </label>`)}
    </div>
  `;
}
