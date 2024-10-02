// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {PrintPreviewNumberSettingsSectionElement} from 'chrome://print/print_preview.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {triggerInputEvent} from './print_preview_test_utils.js';

const number_settings_section_test = {
  suiteName: 'NumberSettingsSectionTest',
  TestNames: {
    BlocksInvalidKeys: 'blocks invalid keys',
    UpdatesErrorMessage: 'updates error message',
  },
};

Object.assign(
    window, {number_settings_section_test: number_settings_section_test});

suite(number_settings_section_test.suiteName, function() {
  let numberSettings: PrintPreviewNumberSettingsSectionElement;
  let parentElement: HTMLElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
        <div>
          <print-preview-number-settings-section
              min-value="1" max-value="100" default-value="50"
              hint-message="incorrect value entered" input-valid>
          </print-preview-number-settings-section>
        </div>`;
    parentElement = document.querySelector('div')!;
    numberSettings =
        document.querySelector('print-preview-number-settings-section')!;
  });

  // Test that key events that would result in invalid values are blocked.
  test(
      number_settings_section_test.TestNames.BlocksInvalidKeys, function() {
        const input = numberSettings.$.userValue;
        /**
         * @param key Key name for the keyboard event that will be fired.
         * @return Promise that resolves when 'keydown' is received by
         *     |parentElement|.
         */
        function sendKeyDownAndReturnPromise(key: string):
            Promise<KeyboardEvent> {
          const whenKeyDown = eventToPromise('keydown', parentElement);
          keyEventOn(input.inputElement, 'keydown', 0, undefined, key);
          return whenKeyDown;
        }

        return sendKeyDownAndReturnPromise('e')
            .then((e: KeyboardEvent) => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise('.');
            })
            .then((e: KeyboardEvent) => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise('-');
            })
            .then((e: KeyboardEvent) => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise('E');
            })
            .then((e: KeyboardEvent) => {
              assertTrue(e.defaultPrevented);
              return sendKeyDownAndReturnPromise('+');
            })
            .then((e: KeyboardEvent) => {
              assertTrue(e.defaultPrevented);
              // Try a valid key.
              return sendKeyDownAndReturnPromise('1');
            })
            .then((e: KeyboardEvent) => {
              assertFalse(e.defaultPrevented);
            });
      });

  test(
      number_settings_section_test.TestNames.UpdatesErrorMessage, function() {
        const input = numberSettings.$.userValue;

        // The error message should be empty initially, since the input is
        // valid.
        assertTrue(numberSettings.inputValid);
        assertEquals('', input.errorMessage);

        // Enter an out of range value, and confirm that the error message is
        // updated correctly.
        return triggerInputEvent(input, '300', numberSettings).then(() => {
          assertFalse(numberSettings.inputValid);
          assertEquals('incorrect value entered', input.errorMessage);
        });
      });
});
