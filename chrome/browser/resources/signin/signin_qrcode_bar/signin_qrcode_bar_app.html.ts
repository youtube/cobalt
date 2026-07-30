// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SigninQrcodeBarAppElement} from './signin_qrcode_bar_app.js';

export function getHtml(this: SigninQrcodeBarAppElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  <h1>QR Code Sign-in Banner</h1>
  <p>This is a placeholder for the QR code banner.</p>
</div>
<!--_html_template_end_-->`;
}
