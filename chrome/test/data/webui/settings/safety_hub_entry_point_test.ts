// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {SettingsSafetyHubEntryPointElement, SafetyHubBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

// clang-format on

suite('SafetyHubEntryPoint', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let page: SettingsSafetyHubEntryPointElement;

  const subheader = 'Passwords, extensions';

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-hub-entry-point');
    document.body.appendChild(page);
    await flushTasks();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
  });

  teardown(function() {
    page.remove();
  });

  test('Safety Hub has recommendations', async function() {
    browserProxy.setSafetyHubHasRecommendations(true);
    browserProxy.setSafetyHubEntryPointSubheader(subheader);
    await createPage();

    assertTrue(page.$.module.hasAttribute('header'));
    assertEquals(
        page.$.module.getAttribute('header')!.trim(),
        page.i18n('safetyHubEntryPointHeader'));
    assertTrue(page.$.module.hasAttribute('subheader'));
    assertEquals(page.$.module.getAttribute('subheader')!.trim(), subheader);
    assertTrue(page.$.module.hasAttribute('header-icon-color'));
    assertEquals(
        page.$.module.getAttribute('header-icon-color')!.trim(), 'blue');

    // Entry point has primary button leading to Safety Hub.
    assertEquals(page.$.button!.getAttribute('class'), 'action-button');
    page.$.button.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SAFETY_HUB);
  });

  test('Safety Hub has no recommendations', async function() {
    browserProxy.setSafetyHubHasRecommendations(false);
    browserProxy.setSafetyHubEntryPointSubheader(
        page.i18n('safetyHubEntryPointNothingToDo'));
    await createPage();

    assertEquals(page.$.module.getAttribute('header')!.trim(), '');
    assertTrue(page.$.module.hasAttribute('subheader'));
    assertEquals(
        page.$.module.getAttribute('subheader')!.trim(),
        page.i18n('safetyHubEntryPointNothingToDo'));
    assertTrue(page.$.module.hasAttribute('header-icon-color'));
    assertEquals(page.$.module.getAttribute('header-icon-color')!.trim(), '');

    // Entry point has secondary button leading to Safety Hub.
    assertEquals(page.$.button!.getAttribute('class'), '');
    page.$.button.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SAFETY_HUB);
  });
});
