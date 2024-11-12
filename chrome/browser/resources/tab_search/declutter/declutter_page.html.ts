// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DeclutterPageElement} from './declutter_page.js';

export function getHtml(this: DeclutterPageElement) {
  return html`<!--_html_template_start_-->
  <div id="declutterPage">
    <div id="header">
      ${
      this.showBackButton ? html`
        <cr-icon-button
            aria-label="${this.getBackButtonAriaLabel_()}"
            iron-icon="cr:arrow-back"
            @click="${this.onBackClick_}">
        </cr-icon-button>
      ` :
                            ''}
      <div id="headerText">
        <div class="title">$i18n{declutterTitle}</div>
        ${this.staleTabDatas_.length === 0 ? '' : html`
          <div class="subheading">$i18n{declutterBody}</div>
        `}
      </div>
    </div>
    ${
      this.staleTabDatas_.length === 0 ? html`
      <div class="empty-content">
        <div class="empty-title">$i18n{declutterEmptyTitle}</div>
        <div class="empty-subheading">$i18n{declutterEmptyBody}</div>
      </div>
    ` :
                                         html`
      <div id="scrollable">
        <div id="staleTabList" class="tabList">
          ${this.staleTabDatas_.map((item, index) => html`
              <tab-search-item class="mwb-list-item" .data="${item}"
                  close-button-aria-label=
                      "${this.getCloseButtonAriaLabel_(item)}"
                  close-button-icon="tab-search:remove"
                  role="option"
                  data-index="${index}"
                  @keydown="${this.onTabKeyDown_}"
                  @close="${this.onTabRemove_}"
                  @focus="${this.onTabFocus_}"
                  @blur="${this.onTabBlur_}"
                  hide-url>
              </tab-search-item>
          `)}
        </div>
        ${
                                             this.dedupeEnabled_ ? html`
          <div id="duplicateTabList" class="tabList">
            Duplicate tab placeholder
          </div>
        ` :
                                                                   ''}
      </div>
      <cr-button class="action-button" @click="${this.onCloseTabsClick_}">
        Close tabs
      </cr-button>
    `}
  </div><!--_html_template_end_-->`;
}
