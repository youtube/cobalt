// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Foldable container for dictionary editor (for a single
 * dictionary).
 */
import './os_japanese_dictionary_entry_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BigString} from 'chrome://resources/mojo/mojo/public/mojom/base/big_string.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {JapaneseDictionary} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';
import {JpPosType} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';

import {getTemplate} from './os_japanese_dictionary_expand.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

interface OsJapaneseDictionaryExpandElement {
  $: {
    selectFileDialog: HTMLElement,
  };
}

class OsJapaneseDictionaryExpandElement extends PolymerElement {
  static get is() {
    return 'os-japanese-dictionary-expand' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dict: {
        type: Object,
      },
      syncedEntriesCount: {
        type: Number,
      },
    };
  }

  // The Japanese Dictionary that this component displays information on.
  dict: JapaneseDictionary;

  // Any entry beyond this index needs to be added to the dictionary rather than
  // "edited" since it does not exist in the file storage at the moment.
  syncedEntriesCount: number;

  // Whether or not this container UI is expanded or folded.
  private expanded_ = false;

  // Adds a new entry locally to create an entry-row component.
  private addEntry_(): void {
    // This changes the entries array from the parent component which it will
    // not be notified of. This is intentional.
    // We do not want to trigger a rerender in the parent component.
    this.push(
        'dict.entries',
        {key: '', value: '', pos: JpPosType.kNoPos, comment: ''});
  }

  // Renames the dictionary.
  private async saveName_(e: Event): Promise<void> {
    this.dict.name = (e.target as CrInputElement).value;
    const dictionarySaved =
        (await UserDataServiceProvider.getRemote().renameJapaneseDictionary(
             this.dict.id, this.dict.name))
            .status.success;
    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  // Renames the dictionary.
  private async deleteDictionary_(): Promise<void> {
    const dictionarySaved =
        (await UserDataServiceProvider.getRemote().deleteJapaneseDictionary(
             this.dict.id))
            .status.success;
    if (dictionarySaved) {
      this.dispatchSavedEvent_();
    }
  }

  // Export dictionary.
  private exportDictionary_(): void {
    const a = document.createElement('a');
    a.href = `jp-export-dictionary/${this.dict.id}`;
    // In case there is no name, use a placeholder name.
    const fileName = this.dict.name || 'unnamed-dictionary';
    a.download = `${fileName}.txt`;
    a.click();
  }

  // Imports dictionary.
  private importDictionary_(): void {
    this.$.selectFileDialog.dispatchEvent(new MouseEvent('click'));
  }

  private async handleFileSelectChange_(e: Event): Promise<void> {
    const fileInput = e.target as HTMLInputElement;
    const fileData = fileInput.files![0];
    // Use bytes for now rather than shared memory for simplicity.
    // TODO(b/366101658): Use shared memory when file is too big.
    // The limit below is the max size that a mojo BigBuffer can handle via
    // directly using the bytes rather than shared memory.
    if (fileData.size >= 128 * 1048576) {
      return;
    }
    const fileDataView = new Uint8Array(await fileData.arrayBuffer());
    const fileMojomBigBuffer: BigBuffer = {
      bytes: Array.from(fileDataView),
      sharedMemory: undefined,
      invalidBuffer: undefined,
    };
    const fileMojomBigString: BigString = {data: fileMojomBigBuffer};

    const {status} =
        await UserDataServiceProvider.getRemote().importJapaneseDictionary(
            this.dict.id, fileMojomBigString);
    if (status.success) {
      this.dispatchSavedEvent_();
    }
  }

  // Returns true if this entry is a locally added entry.
  private locallyAdded_(entryIndex: number): boolean {
    // This entry falls outside of the range of entries that were initially
    // synced, hence it must be added locally.
    return entryIndex > this.syncedEntriesCount;
  }

  // If there is currently an unsynced entry then hide the add button.
  // This is to prevent two "unadded" entries to cause issues with ordering when
  // synced. Users should only be able to add one entry at a time before a sync
  // occurs.
  private shouldShowAddButton_(entriesLength: number): boolean {
    return entriesLength - 1 <= this.syncedEntriesCount;
  }

  private dispatchSavedEvent_(): void {
    this.dispatchEvent(
        new CustomEvent('dictionary-saved', {bubbles: true, composed: true}));
  }
}

customElements.define(
    OsJapaneseDictionaryExpandElement.is, OsJapaneseDictionaryExpandElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsJapaneseDictionaryExpandElement.is]: OsJapaneseDictionaryExpandElement;
  }
}

declare global {
  interface HTMLElementEventMap {
    ['dictionary-saved']: CustomEvent;
  }
}
