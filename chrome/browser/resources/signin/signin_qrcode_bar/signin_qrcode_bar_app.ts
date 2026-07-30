// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signin_qrcode_bar_app.css.js';
import {getHtml} from './signin_qrcode_bar_app.html.js';

export class SigninQrcodeBarAppElement extends CrLitElement {
  static get is() {
    return 'signin-qrcode-bar-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signin-qrcode-bar-app': SigninQrcodeBarAppElement;
  }
}

customElements.define(SigninQrcodeBarAppElement.is, SigninQrcodeBarAppElement);
