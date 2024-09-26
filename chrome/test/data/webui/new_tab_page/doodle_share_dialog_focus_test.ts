// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {DoodleShareDialogElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('NewTabPageDoodleShareDialogFocusTest', () => {
  let doodleShareDialog: DoodleShareDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    doodleShareDialog = document.createElement('ntp-doodle-share-dialog');
    document.body.appendChild(doodleShareDialog);
  });

  test('clicking copy copies URL', async () => {
    // Arrange.
    doodleShareDialog.url = {url: 'https://bar.com'};

    // Act.
    doodleShareDialog.$.copyButton.click();

    // Assert.
    const text = await navigator.clipboard.readText();
    assertEquals(text, 'https://bar.com');
  });
});
