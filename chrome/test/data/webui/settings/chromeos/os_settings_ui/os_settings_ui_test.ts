// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrDrawerElement, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsUiElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @fileoverview Suite of tests for the OS Settings ui and main page. */

suite('OSSettingsUi', function() {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement|null;
  let mainPageContainer: MainPageContainerElement|null;

  suiteSetup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    settingsMain = ui.shadowRoot!.querySelector('os-settings-main');
    assert(settingsMain);

    mainPageContainer =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assert(mainPageContainer);

    const idleRender =
        mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  });

  test('Update required end of life banner visibility', function() {
    flush();
    assert(mainPageContainer);
    assertEquals(
        null,
        mainPageContainer.shadowRoot!.querySelector(
            '#updateRequiredEolBanner'));

    mainPageContainer!.set('showUpdateRequiredEolBanner_', true);
    flush();
    assertTrue(!!mainPageContainer.shadowRoot!.querySelector(
        '#updateRequiredEolBanner'));
  });

  test('Update required end of life banner close button click', function() {
    assert(mainPageContainer);
    mainPageContainer.set('showUpdateRequiredEolBanner_', true);
    flush();
    const banner = mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
        '#updateRequiredEolBanner');
    assertTrue(!!banner);

    const closeButton =
        mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
            '#closeUpdateRequiredEol');
    assert(closeButton);
    closeButton.click();
    flush();
    assertEquals('none', banner.style.display);
  });

  test('clicking icon closes drawer', async () => {
    flush();
    const drawer = ui.shadowRoot!.querySelector<CrDrawerElement>('#drawer');
    assert(drawer);
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);

    // Clicking the drawer icon closes the drawer.
    ui.shadowRoot!.querySelector<HTMLElement>('#iconButton')!.click();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });
});
