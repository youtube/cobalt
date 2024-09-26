// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface NetworkIpConfigElement extends LegacyElementMixin, HTMLElement {
  getAutoConfigIpToggle(): CrToggleElement|null;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-ip-config': NetworkIpConfigElement;
  }
}

export {NetworkIpConfigElement};
