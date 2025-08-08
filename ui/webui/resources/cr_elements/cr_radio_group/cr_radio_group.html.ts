// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrRadioGroupElement} from './cr_radio_group.js';

export function getHtml(this: CrRadioGroupElement) {
  return html`<slot></slot>`;
}
