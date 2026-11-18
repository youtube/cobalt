// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BookmarksElement} from './bookmarks.js';

export function getHtml(this: BookmarksElement) {
  // clang-format off
  return html`
<div id="container">
  <webui-browser-bookmark-tree-node .node="${this.rootNode_}">
  </webui-browser-bookmark-tree-node>
</div>
`;
  // clang-format on
}
