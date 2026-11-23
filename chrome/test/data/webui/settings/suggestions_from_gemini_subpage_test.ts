// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsSuggestionsFromGeminiSubpageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      isAtMemoryEnabled: true,
    });

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    subpage =
        document.createElement('settings-suggestions-from-gemini-subpage');
    subpage.prefs = {
      autofill: {
        personal_context: {
          settings_toggle_status: {
            key: 'autofill.personal_context.settings_toggle_status',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
      },
    };
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

  test('QualityLoggingRendersExpectedColumnsAndBullets', function() {
    const columns = subpage.shadowRoot!.querySelectorAll('.column');
    assertEquals(2, columns.length);

    const firstColumn = columns[0]!;
    const firstColumnBullets = firstColumn.querySelectorAll('li');
    assertEquals(2, firstColumnBullets.length);
    assertEquals(
        'settings20:finance',
        firstColumnBullets[0]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiWhenUsed1'),
        firstColumnBullets[0]!.querySelector(
                                  '.cr-secondary-text')!.textContent.trim());
    assertEquals(
        'settings20:personal-recommendations',
        firstColumnBullets[1]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiWhenUsed2'),
        firstColumnBullets[1]!.querySelector(
                                  '.cr-secondary-text')!.textContent.trim());

    const secondColumn = columns[1]!;
    const secondColumnBullets = secondColumn.querySelectorAll('li');
    assertEquals(3, secondColumnBullets.length);
    assertEquals(
        'settings20:insight-spark',
        secondColumnBullets[0]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider1'),
        secondColumnBullets[0]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());
    assertEquals(
        'settings20:account-box',
        secondColumnBullets[1]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider2'),
        secondColumnBullets[1]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());
    assertEquals(
        'cr20:domain', secondColumnBullets[2]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider3'),
        secondColumnBullets[2]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());
  });

  test('QualityLoggingIsHiddenWhenToggleIsOff', async function() {
    assertTrue(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));

    subpage.set(
        'prefs.autofill.personal_context.settings_toggle_status.value', false);
    await flushTasks();

    assertFalse(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));
  });

  test('QualityLoggingIsHiddenWhenAtMemoryDisabled', async function() {
    assertTrue(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));

    subpage.set('isAtMemoryEnabled_', false);
    await flushTasks();

    assertFalse(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));
  });
});
