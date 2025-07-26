// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsErrorPageElement} from './error_page.js';

export function getHtml(this: ExtensionsErrorPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="page-container" id="container">
  <div class="page-content">
    <div id="heading" class="cr-title-text">
      <cr-icon-button class="icon-arrow-back no-overlap" id="closeButton"
          aria-label="$i18n{back}" @click="${this.onCloseButtonClick_}">
      </cr-icon-button>
      <span role="heading" aria-level="2">$i18n{errorsPageHeading}</span>
      ${this.showReloadButton_() ? html`
        <cr-icon-button id="dev-reload-button" class="icon-refresh no-overlap"
            title="$i18n{itemReload}" aria-label="$i18n{itemReload}"
            aria-describedby="a11yAssociation" @click="${this.onReloadClick_}">
        </cr-icon-button>` : ''}
      <cr-button @click="${this.onClearAllClick_}"
          ?hidden="${!this.entries_.length}">
        $i18n{clearAll}
      </cr-button>
    </div>
    <div class="section">
      <div id="errorsList">
        ${this.entries_.map((entry, index) => html`
          <div class="item-container">
            <div class="cr-row error-item ${this.computeErrorClass_(index)}">
              <div actionable class="start" @click="${this.onErrorItemAction_}"
                  @keydown="${this.onErrorItemAction_}" tabindex="0"
                  data-error-index="${index}" role="button"
                  aria-expanded="${this.isAriaExpanded_(index)}">
                <cr-icon .icon="${this.computeErrorIcon_(entry)}"
                    title="${this.computeErrorTypeLabel_(entry)}">
                </cr-icon>
                <div id="${entry.id}" class="error-message">
                  ${entry.message}
                </div>
                <div class="cr-icon ${this.iconName_(index)}">
                </div>
              </div>
              <div class="separator"></div>
              <cr-icon-button class="icon-delete-gray"
                  data-error-id="${entry.id}"
                  @click="${this.onDeleteErrorAction_}"
                  aria-describedby="${entry.id}" aria-label="$i18n{clearEntry}">
              </cr-icon-button>
            </div>
            <cr-collapse ?opened="${this.isOpened_(index)}">
              <div class="devtools-controls">
                ${this.computeIsRuntimeError_(entry) ? html`
                  <div class="details-heading cr-title-text" role="heading"
                      aria-level="3">
                    $i18n{errorContext}
                  </div>
                  <span class="context-url">
                    ${this.getContextUrl_(entry)}
                  </span>
                  <div class="details-heading cr-title-text" role="heading"
                      aria-level="3">
                    $i18n{stackTrace}
                  </div>
                  <ul class="stack-trace-container" data-error-index="${index}"
                      @keydown="${this.onStackKeydown_}">
                    ${(entry as chrome.developerPrivate.RuntimeError)
                        .stackTrace.map((item, frameIndex) => html`
                      <li @click="${this.onStackFrameClick_}"
                          data-frame-index="${frameIndex}"
                          data-error-index="${index}"
                          tabindex="${this.getStackFrameTabIndex_(item)}"
                          ?hidden="${!this.shouldDisplayFrame_(item.url)}"
                          class="${this.getStackFrameClass_(item)}">
                        ${this.getStackTraceLabel_(item)}
                      </li>`)}
                  </ul>` : ''}
                <extensions-code-section .code="${this.code_}"
                    ?is-active="${this.isOpened_(index)}"
                    could-not-display-code="$i18n{noErrorsToShow}">
                </extensions-code-section>
              </div>
            </cr-collapse>
          </div>`)}
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
