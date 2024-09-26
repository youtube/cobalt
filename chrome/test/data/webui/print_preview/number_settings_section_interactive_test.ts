// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {PrintPreviewNumberSettingsSectionElement} from 'chrome://print/print_preview.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {triggerInputEvent} from './print_preview_test_utils.js';

const number_settings_section_interactive_test = {
  suiteName: 'NumberSettingsSectionInteractiveTest',
  TestNames: {
    BlurResetsEmptyInput: 'blur resets empty input',
  },
};

Object.assign(window, {
  number_settings_section_interactive_test:
      number_settings_section_interactive_test,
});

suite(number_settings_section_interactive_test.suiteName, function() {
  let numberSettings: PrintPreviewNumberSettingsSectionElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
          <print-preview-number-settings-section
              min-value="1" max-value="100" default-value="50"
              current-value="10" hint-message="incorrect value entered"
              input-valid>
          </print-preview-number-settings-section>`;
    numberSettings =
        document.querySelector('print-preview-number-settings-section')!;
  });

  // Verifies that blurring the input will reset it to the default if it is
  // empty, but not if it contains an invalid value.
  test(
      number_settings_section_interactive_test.TestNames.BlurResetsEmptyInput,
      async () => {
        // Initial value is 10.
        const crInput = numberSettings.getInput();
        const input = crInput.inputElement;
        assertEquals('10', input.value);

        // Set something invalid in the input.
        input.focus();
        await triggerInputEvent(input, '0', numberSettings);
        assertEquals('0', input.value);
        assertTrue(crInput.invalid);

        // Blurring the input does not clear it or clear the error if there
        // is an explicit invalid value.
        input.blur();
        assertEquals('0', input.value);
        assertTrue(crInput.invalid);

        // Clear the input.
        input.focus();

        await triggerInputEvent(input, '', numberSettings);
        assertEquals('', input.value);
        assertFalse(crInput.invalid);

        // Blurring the input clears it to the default when it is empty.
        input.blur();
        assertEquals('50', input.value);
        assertFalse(crInput.invalid);
      });
});
