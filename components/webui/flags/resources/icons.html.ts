// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="flags" size="24">
  <svg>
    <defs>
      <g id="file-upload" viewBox="0 -960 960 960">
        <path d="M440-320v-326L336-542l-56-58 200-200 200 200-56 58-104-104v326h-80ZM240-160q-33 0-56.5-23.5T160-240v-120h80v120h480v-120h80v120q0 33-23.5 56.5T720-160H240Z"></path>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
