// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks.css.js';
import {getHtml} from './memory_banks.html.js';

export class MemoryBanksElement extends CrLitElement {
  static get is() {
    return 'memory-banks';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries: {type: Array},
      selectedIds: {type: Object},
    };
  }

  accessor entries: MemoryBankEntry[] = [];
  accessor selectedIds: Set<bigint> = new Set();

  override connectedCallback() {
    super.connectedCallback();
    this.fetchEntries();
  }

  private async fetchEntries() {
    const {entries} =
        await BrowserProxyImpl.getInstance().handler.getAllEntries();
    this.entries = entries;
  }

  protected get recentlySaved_(): MemoryBankEntry[] {
    return this.entries.slice(0, 3);
  }

  protected convertMojoTimeToDate_(mojoTime: {internalValue: bigint}): Date {
    // Mojo Time represents microseconds since the Windows epoch (January 1,
    // 1601). JavaScript Date expects milliseconds since the Unix epoch (January
    // 1, 1970).

    // 11,644,473,600,000,000n is the Windows-to-Unix epoch delta in
    // microseconds.
    const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
    return new Date(Number(unixEpochUs / 1000n));
  }

  protected isSelected_(id: bigint): boolean {
    return this.selectedIds.has(id);
  }

  protected isAllSelected_(): boolean {
    return this.entries.length > 0 &&
        this.selectedIds.size === this.entries.length;
  }

  protected isSomeSelected_(): boolean {
    return this.selectedIds.size > 0 &&
        this.selectedIds.size < this.entries.length;
  }

  protected onCheckboxClick_(e: Event) {
    e.stopPropagation();
  }

  protected onCheckboxChange_(e: Event) {
    const checkbox = e.target as HTMLElement & {checked: boolean};
    const id = BigInt(checkbox.dataset['id']!);
    if (checkbox.checked) {
      this.selectedIds.add(id);
    } else {
      this.selectedIds.delete(id);
    }
    this.selectedIds = new Set(this.selectedIds);
  }

  protected onSelectAllChange_(e: Event) {
    const checkbox = e.target as HTMLElement & {checked: boolean};
    if (checkbox.checked) {
      this.selectedIds = new Set(this.entries.map(entry => entry.id));
    } else {
      this.selectedIds.clear();
      this.selectedIds = new Set(this.selectedIds);
    }
  }

  protected async onCopyClick_() {
    const textToCopy = this.getSelectedEntriesAsText_();
    try {
      await navigator.clipboard.writeText(textToCopy);
    } catch (err) {
      console.error('Failed to copy: ', err);
    }
  }

  protected onDownloadClick_() {
    const textToDownload = this.getSelectedEntriesAsText_();
    const blob = new Blob([textToDownload], {type: 'text/plain;charset=utf-8'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'memory_banks_entries.txt';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  private getSelectedEntriesAsText_(): string {
    const selectedEntries =
        this.entries.filter(entry => this.selectedIds.has(entry.id));
    return selectedEntries
        .map(entry => {
          const dateStr =
              this.convertMojoTimeToDate_(entry.timestamp).toLocaleString();
          if (entry.type === EntryType.kTextSelection) {
            return `[Text Selection] "${
                entry.selectedText || ''}"\nPage Title: ${
                entry.tabTitle}\nURL: ${entry.url}\nSaved: ${dateStr}\n`;
          } else {
            return `[Tab] Title: ${entry.tabTitle}\nURL: ${entry.url}\nSaved: ${
                dateStr}\n`;
          }
        })
        .join('\n---\n\n');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks': MemoryBanksElement;
  }
}

customElements.define(MemoryBanksElement.is, MemoryBanksElement);
