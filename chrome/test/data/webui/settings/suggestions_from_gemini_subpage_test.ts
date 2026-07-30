// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsSuggestionsFromGeminiSubpageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SuggestionsFromGeminiSubpage', function() {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;

  let subpage: SettingsSuggestionsFromGeminiSubpageElement;
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    loadTimeData.overrideValues({
      personalContextConnectedAppsUrl: 'https://gemini.google.com/apps',
    });

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    subpage =
        document.createElement('settings-suggestions-from-gemini-subpage');
    document.body.appendChild(subpage);
    return flushTasks();
  });

  test('ManageConnectedAppsClick', async function() {
    const row = subpage.shadowRoot!.querySelector<HTMLElement>(
        '#manageConnectedAppsLinkRow');
    assertTrue(!!row);
    assertTrue(isVisible(row));

    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('personalContextConnectedAppsUrl'), url);
  });
});
