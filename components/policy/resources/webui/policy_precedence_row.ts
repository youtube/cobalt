// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './policy_precedence_row.css.js';
import {getHtml} from './policy_precedence_row.html.js';

export class PolicyPrecedenceRowElement extends CrLitElement {
  static get is() {
    return 'policy-precedence-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      precedenceOrder: {type: Array},
    };
  }

  accessor precedenceOrder: string[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'rowgroup');
    this.classList.add('policy-precedence-data');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-precedence-row': PolicyPrecedenceRowElement;
  }
}

customElements.define(
    PolicyPrecedenceRowElement.is, PolicyPrecedenceRowElement);
