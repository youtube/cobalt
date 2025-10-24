// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsPackDialogElement} from './pack_dialog.js';

export function getHtml(this: ExtensionsPackDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">$i18n{packDialogTitle}</div>
  <div slot="body">
    <div>$i18n{packDialogContent}</div>
    <cr-input id="rootDir" label="$i18n{packDialogExtensionRoot}"
        .value="${this.packDirectory_}"
        @value-changed="${this.onPackDirectoryChanged_}" autofocus>
      <cr-button id="rootDirBrowse" @click="${this.onRootBrowse_}"
          slot="suffix">
        $i18n{packDialogBrowse}
      </cr-button>
    </cr-input>
    <cr-input id="keyFile" label="$i18n{packDialogKeyFile}"
        .value="${this.keyFile_}" @value-changed="${this.onKeyFileChanged_}">
      <cr-button id="keyFileBrowse" @click="${this.onKeyBrowse_}" slot="suffix">
        $i18n{packDialogBrowse}
      </cr-button>
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onConfirmClick_}"
        ?disabled="${!this.packDirectory_}">
      $i18n{packDialogConfirm}
    </cr-button>
  </div>
</cr-dialog>
${this.lastResponse_ ? html`
  <extensions-pack-dialog-alert .model="${this.lastResponse_}"
      @close="${this.onAlertClose_}">
  </extensions-pack-dialog-alert>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
