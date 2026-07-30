// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {EntryType} from '../context_hub.mojom-webui.js';

import type {MemoryBanksElement} from './memory_banks.js';

export function getHtml(this: MemoryBanksElement) {
  return html`
    <main id="memory-banks-view">
        <section>
            <div class="header-container">
              <h1>Memory banks</h1>
            </div>

            ${
      this.entries.length === 0 ?
          '' :
          html`
              <div class="action-bar">
                <cr-checkbox
                    ?checked="${this.isAllSelected_()}"
                    ?indeterminate="${this.isSomeSelected_()}"
                    @change="${this.onSelectAllChange_}">
                  Select all
                </cr-checkbox>
                <div class="action-buttons">
                  <cr-button ?disabled="${
              this.selectedIds.size === 0}" @click="${this.onCopyClick_}">
                    Copy selected
                  </cr-button>
                  <cr-button ?disabled="${
              this.selectedIds.size === 0}" @click="${this.onDownloadClick_}">
                    Download selected
                  </cr-button>
                </div>
              </div>
            `}
            ${
      this.entries.length === 0 ?
          html`
              <p>No saved memories yet.</p>
            ` :
          html`
              <h2>Recently saved</h2>
              <div class="grid">
                ${
              this.recentlySaved_.map(
                  entry => html`
                  <a class="card ${
                      this.isSelected_(entry.id) ?
                          'selected' :
                          ''}" href="${entry.url}" target="_blank">
                    <cr-checkbox class="card-checkbox"
                        data-id="${entry.id}"
                        ?checked="${this.isSelected_(entry.id)}"
                        @change="${this.onCheckboxChange_}"
                        @click="${this.onCheckboxClick_}">
                    </cr-checkbox>
                    ${
                      entry.type === EntryType.kTextSelection ?
                          html`
                      <div class="card-body">
                        <p class="text-preview">"${
                              entry.selectedText || ''}"</p>
                      </div>
                    ` :
                          html`
                      <div class="card-body tab-type">
                        <cr-icon icon="cr:insert-drive-file"></cr-icon>
                      </div>
                    `}
                    <div class="card-footer">
                      <div class="favicon"
                          style="background-image: ${
                      getFaviconForPageURL(entry.url, true)}">
                      </div>
                      <div class="meta-text">
                        <span class="card-title">${entry.tabTitle}</span>
                        <span class="card-date">
                          ${
                      this.convertMojoTimeToDate_(entry.timestamp)
                          .toLocaleDateString(undefined, {
                            month: 'short',
                            day: 'numeric',
                            year: 'numeric',
                          })}
                        </span>
                      </div>
                    </div>
                  </a>
                `)}
              </div>

              <h2>All saved</h2>
              <div class="grid">
                ${
              this.entries.map(
                  entry => html`
                  <a class="card ${
                      this.isSelected_(entry.id) ?
                          'selected' :
                          ''}" href="${entry.url}" target="_blank">
                    <cr-checkbox class="card-checkbox"
                        data-id="${entry.id}"
                        ?checked="${this.isSelected_(entry.id)}"
                        @change="${this.onCheckboxChange_}"
                        @click="${this.onCheckboxClick_}">
                    </cr-checkbox>
                    ${
                      entry.type === EntryType.kTextSelection ?
                          html`
                      <div class="card-body">
                        <p class="text-preview">"${
                              entry.selectedText || ''}"</p>
                      </div>
                    ` :
                          html`
                      <div class="card-body tab-type">
                        <cr-icon icon="cr:insert-drive-file"></cr-icon>
                      </div>
                    `}
                    <div class="card-footer">
                      <div class="favicon"
                          style="background-image: ${
                      getFaviconForPageURL(entry.url, true)}">
                      </div>
                      <div class="meta-text">
                        <span class="card-title">${entry.tabTitle}</span>
                        <span class="card-date">
                          ${
                      this.convertMojoTimeToDate_(entry.timestamp)
                          .toLocaleDateString(undefined, {
                            month: 'short',
                            day: 'numeric',
                            year: 'numeric',
                          })}
                        </span>
                      </div>
                    </div>
                  </a>
                `)}
              </div>
            `}
        </section>
    </main>
  `;
}
