// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualCueingInternalsAppElement} from './app.js';

export function getHtml(this: ContextualCueingInternalsAppElement) {
  return html`
<h1>Contextual Cueing Internals</h1>
<cr-tab-box>
  <div slot="tab">Generated Cues</div>
  <div slot="tab">Logs</div>
  <div slot="panel">
    <div class="card">
      <h2>Generated Cues</h2>
    </div>
  </div>
  <div slot="panel">
    <div class="card">
      <h2>Logs Panel</h2>
    </div>
  </div>
</cr-tab-box>`;
}
