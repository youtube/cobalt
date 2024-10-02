// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiSearch} from 'chrome://emoji-picker/emoji_search.js';
import {EMOJI_TEXT_BUTTON_CLICK} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertGT} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest, waitForCondition, waitWithTimeout} from './emoji_picker_test_util.js';

suite('emoji-search', () => {
  let emojiPicker: EmojiPicker;
  let emojiSearch: EmojiSearch;
  let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest();
    emojiPicker = newPicker.emojiPicker;
    findInEmojiPicker = newPicker.findInEmojiPicker;
    await newPicker.readyPromise;
    emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
  });


  test('By default, there is no search result.', () => {
    const searchResults = findInEmojiPicker('emoji-search', '#results');
    assert(!searchResults);
  });

  test(
      'If matching, search should return both emoji and emoticon results.',
      async () => {
        emojiSearch.setSearchQuery('face');
        await waitForCondition(
            () => findInEmojiPicker(
                'emoji-search', 'emoji-group[category="emoji"]'),
            'wait for search results to enter');
        const emojiResults =
            findInEmojiPicker(
                'emoji-search',
                'emoji-group')!.shadowRoot!.querySelectorAll('.emoji-button');
        assertGT(emojiResults.length, 0);
        const emoticonResults =
            findInEmojiPicker(
                'emoji-search', 'emoji-group[category="emoticon"]')!.shadowRoot!
                .querySelectorAll('.emoji-button');
        assertGT(emoticonResults.length, 0);
      });

  test(
      'Search should display meaningful output when no result is found.',
      async () => {
        emojiSearch.setSearchQuery('zyxt');
        await waitForCondition(
            () => findInEmojiPicker('emoji-search', '.no-result'),
            'wait for no result to render');
        const message = findInEmojiPicker('emoji-search', '.no-result');
        assertEquals(message!.innerText, 'No result found');
      });

  test(
      'If there is only one emoji returned, pressing Enter triggers the ' +
          'clicking event.',
      async () => {
        emojiSearch.setSearchQuery('zombi');
        await waitForCondition(
            () => findInEmojiPicker('emoji-search', 'emoji-group'),
            'wait for search to render');
        const enterEvent = new KeyboardEvent(
            'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
        const buttonClickPromise = new Promise<void>(
            (resolve) => emojiPicker.addEventListener(
                EMOJI_TEXT_BUTTON_CLICK, (event) => {
                  assertEquals('🧟', event.detail.text);
                  assertEquals('zombie', event.detail.name.trim());
                  resolve();
                }));
        emojiSearch.onSearchKeyDown(enterEvent);
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Failed to receive emoji button click event.');
      });

  test(
      'If there is only emoticon returned, pressing Enter triggers the ' +
          'clicking event.',
      async () => {
        emojiSearch.setSearchQuery('cat');
        await waitForCondition(
            () => findInEmojiPicker('emoji-search', 'emoji-group'),
            'wait for search to be rendered');
        const enterEvent = new KeyboardEvent(
            'keydown', {cancelable: true, key: 'Enter', keyCode: 13});

        const buttonClickPromise = new Promise<void>(
            (resolve) => emojiPicker.addEventListener(
                EMOJI_TEXT_BUTTON_CLICK, (event) => {
                  assertEquals('=^.^=', event.detail.text);
                  resolve();
                }));

        emojiSearch.onSearchKeyDown(enterEvent);
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Failed to receive emoji button click event.');
      });

  test('Search only emoji groups are discoverable through search', async () => {
    emojiSearch.setSearchQuery('gamma');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group'),
        'wait for search group to be rendered');
    const emojiResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="symbol"]')!
            .shadowRoot!.querySelectorAll('.emoji-button');
    assertEquals(
        emojiResults!.length,
        4,  // normal, italic, bold and san-serif bold
    );
  });
});
