// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {HistorySearchedLabelElement} from 'chrome://history/history.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('<history-searched-label> unit test', function() {
  let label: HistorySearchedLabelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    label = document.createElement('history-searched-label');
    document.body.appendChild(label);
  });

  test('matching query sets bold', function() {
    assertEquals(0, document.querySelectorAll('b').length);
    // Note: When the page is reloaded with a search query, |searchTerm| will be
    // initialized before |title|. Keep this ordering as a regression test for
    // https://crbug.com/921455.
    label.searchTerm = 'f';
    label.title = 'foo';

    flush();
    const boldItems = document.querySelectorAll('b');
    assertEquals(1, boldItems.length);
    assertEquals(label.searchTerm, boldItems[0]!.textContent);

    label.searchTerm = 'g';
    flush();
    assertEquals(0, document.querySelectorAll('b').length);
  });
});
