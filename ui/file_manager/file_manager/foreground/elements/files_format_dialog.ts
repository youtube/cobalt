// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntryList} from '../../common/js/files_app_entry_types.js';
import {isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {bytesToString, str, strf} from '../../common/js/translations.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {validateExternalDriveName} from '../js/file_rename.js';

import {getTemplate} from './files_format_dialog.html.js';

export interface FilesFormatDialog {
  $: {
    dialog: CrDialogElement,
    label: CrInputElement,
    'warning-container': HTMLDivElement,
  };
  label_: string;
  formatType_: chrome.fileManagerPrivate.FormatFileSystemType;
  spaceUsed_: string;
  isErase_: boolean;
}

function getVolumeInfoDisplayRoot(entry: Entry|FilesAppEntry): DirectoryEntry|
    null {
  if ('volumeInfo' in entry) {
    return (entry.volumeInfo as VolumeInfo).displayRoot || null;
  }
  return null;
}

export class FilesFormatDialog extends PolymerElement {
  static get is() {
    return 'files-format-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label_: {
        type: String,
        value: '',
      },

      formatType_: {
        type: String,
        value: chrome.fileManagerPrivate.FormatFileSystemType.VFAT,
      },

      spaceUsed_: {
        type: String,
        value: '',
      },

      isErase_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private volumeInfo_: VolumeInfo|null = null;
  private root_: EntryList|null = null;

  override ready() {
    super.ready();

    this.$.dialog.consumeKeydownEvent = true;
  }

  cancel() {
    this.$.dialog.cancel();
  }

  format() {
    try {
      validateExternalDriveName(this.label_, this.formatType_);
    } catch (error: any) {
      this.$.label.setAttribute('error-message', error.message);
      this.$.label.invalid = true;
      return;
    }

    if (this.isErase_) {
      chrome.fileManagerPrivate.singlePartitionFormat(
          this.root_?.devicePath || '', this.formatType_, this.label_);
    } else {
      chrome.fileManagerPrivate.formatVolume(
          this.volumeInfo_?.volumeId || '', this.formatType_, this.label_);
    }
    this.$.dialog.close();
  }


  /**
   * Used to set "single-partition-format" attribute on element.
   * It is used to check flag status in the tests.
   */
  getSinglePartitionFormat() {
    if (isSinglePartitionFormatEnabled()) {
      return 'single-partition-format';
    }
    return '';
  }

  getConfirmLabel(isErase: boolean) {
    if (isSinglePartitionFormatEnabled()) {
      if (isErase) {
        return str('REPARTITION_DIALOG_CONFIRM_LABEL');
      } else {
        return str('FORMAT_DIALOG_CONFIRM_SHORT_LABEL');
      }
    } else {
      return str('FORMAT_DIALOG_CONFIRM_LABEL');
    }
  }

  getDialogMessage(isErase: boolean) {
    if (isSinglePartitionFormatEnabled()) {
      if (isErase) {
        return str('REPARTITION_DIALOG_MESSAGE');
      } else {
        return str('FORMAT_PARTITION_DIALOG_MESSAGE');
      }
    } else {
      return str('FORMAT_DIALOG_MESSAGE');
    }
  }

  getStrf(token: string, value: string): string {
    return strf(token, value);
  }

  /**
   * Shows the dialog for drive represented by |volumeInfo|.
   */
  showModal(volumeInfo: VolumeInfo) {
    this.isErase_ = false;
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.spaceUsed_ = '';

    this.volumeInfo_ = volumeInfo;
    this.title = this.volumeInfo_.label;
    if (volumeInfo.displayRoot) {
      chrome.fileManagerPrivate.getDirectorySize(
          volumeInfo.displayRoot, (spaceUsed: number) => {
            if (spaceUsed > 0 && volumeInfo === this.volumeInfo_) {
              this.spaceUsed_ = bytesToString(spaceUsed);
            }
            if (window.IN_TEST) {
              this.$['warning-container'].setAttribute('fully-initialized', '');
            }
          });
    }

    this.$.dialog.showModal();
  }

  /**
   * Shows the dialog for erasing device.
   */
  showEraseModal(root: EntryList) {
    this.isErase_ = true;
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.spaceUsed_ = '';

    this.root_ = root;
    this.title = root.label;
    const childVolumes = this.root_.getUIChildren();
    let totalSpaceUsed = 0;

    const getSpaceUsedRequests = childVolumes.map((childVolume) => {
      return new Promise((resolve: (value: void) => void) => {
        const displayRoot = getVolumeInfoDisplayRoot(childVolume);
        if (displayRoot) {
          chrome.fileManagerPrivate.getDirectorySize(
              displayRoot, (spaceUsed: number) => {
                totalSpaceUsed += spaceUsed;
                if (totalSpaceUsed > 0) {
                  this.spaceUsed_ = bytesToString(totalSpaceUsed);
                }
                resolve();
              });
        }
      });
    });

    Promise.all(getSpaceUsedRequests).then(() => {
      if (window.IN_TEST) {
        this.$['warning-container'].setAttribute('fully-initialized', '');
      }
    });
    this.$.dialog.showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FilesFormatDialog.is]: FilesFormatDialog;
  }
}

customElements.define(FilesFormatDialog.is, FilesFormatDialog);
