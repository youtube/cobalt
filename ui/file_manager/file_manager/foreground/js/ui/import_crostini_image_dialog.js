// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, util} from '../../../common/js/util.js';

import {ConfirmDialog} from './dialogs.js';



/**
 * ImportCrostiniImageDialog is used as the handler for .tini files.
 */
  /**
   * Creates dialog in DOM.
   */
export class ImportCrostiniImageDialog extends ConfirmDialog {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);
    super.setOkLabel(str('IMPORT_CROSTINI_IMAGE_DIALOG_OK_LABEL'));

    this.container.classList.add('files-ng');
  }

  /**
   * Shows the dialog.
   *
   * @param {!Entry} entry
   */
  showImportCrostiniImageDialog(entry) {
    super.showWithTitle(
        str('IMPORT_CROSTINI_IMAGE_DIALOG_TITLE'),
        str('IMPORT_CROSTINI_IMAGE_DIALOG_DESCRIPTION'),
        chrome.fileManagerPrivate.importCrostiniImage.bind(
            null, /** @type {!Entry} */ (util.unwrapEntry(entry))));
  }
}
