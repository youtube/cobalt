// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewPagesPerSheetSettingsElement} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('PagesPerSheetSettingsTest', function() {
  let pagesPerSheetSection: PrintPreviewPagesPerSheetSettingsElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    pagesPerSheetSection =
        document.createElement('print-preview-pages-per-sheet-settings');
    pagesPerSheetSection.disabled = false;
    document.body.appendChild(pagesPerSheetSection);
    return microtasksFinished();
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = pagesPerSheetSection.shadowRoot.querySelector('select')!;
    assertEquals('1', select.value);

    pagesPerSheetSection.setSetting('pagesPerSheet', 4);
    await microtasksFinished();
    assertEquals('4', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = pagesPerSheetSection.shadowRoot.querySelector('select')!;
    assertEquals('1', select.value);
    assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    assertFalse(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
    assertEquals(6, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(pagesPerSheetSection, '2');
    assertEquals(2, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    assertTrue(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
  });
});
