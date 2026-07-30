// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyPrecedenceRowElement} from './policy_precedence_row.js';

export function getHtml(this: PolicyPrecedenceRowElement) {
  return html`<!--_html_template_start_-->
<div class="precedence row" role="row">
  <div class="name" role="rowheader">$i18n{labelPrecedence}</div>
  <div class="value" role="cell">${this.precedenceOrder.join(' > ')}</div>
</div>
<!--_html_template_end_-->`;
}
