// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/icons.html.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AvatarButtonElement} from './avatar_button.js';

export function getHtml(this: AvatarButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-button id="button"
    class="${this.getButtonClass_()}"
    ?disabled="${!this.state.enabled}"
    ?has-border="${this.shouldPaintBorder()}"
    title="${this.getTooltip_() || ''}"
    aria-label="${this.state.accessibilityName || ''}"
    aria-description="${this.state.accessibilityDescription || ''}"
    @click="${this.onClick_}"
    @mouseenter="${this.onMouseenter_}"
    @mouseleave="${this.onMouseleave_}"
    @focus="${this.onFocus_}"
    @blur="${this.onBlur_}">
  ${(this.state.icon?.handleId ?? 0n) !== 0n ? html`
    <icon-from-table slot="prefix-icon" id="icon"
        .iconHandle="${this.state.icon}"></icon-from-table>
  ` : html`
    <cr-icon slot="prefix-icon" id="icon" icon="cr:person"></cr-icon>
  `}
  <span id="text" ?visible="${!!this.state.text}">
    ${this.state.text || ''}
  </span>
</cr-button>
<!--_html_template_end_-->`;
  // clang-format on
}
