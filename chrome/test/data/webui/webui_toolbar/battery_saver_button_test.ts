// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, ContextMenuType} from 'chrome://webui-toolbar.top-chrome/app.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['showContextMenu']);
  }

  showContextMenu(menuType: number, bounds: any, source: number) {
    this.methodCalled('showContextMenu', {menuType, bounds, source});
  }
}

class TestBatterySaverBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;

  constructor() {
    super([]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
  }
}

suite('BatterySaverButton', function() {
  let button: any;
  let browserProxy: TestBatterySaverBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestBatterySaverBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy as any);

    button = document.createElement('battery-saver-button');
    document.body.appendChild(button);
    await button.updateComplete;
    await microtasksFinished();
  });

  test('ClickShowsBubble', async () => {
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));

    assertTrue(!!button.shadowRoot, 'shadowRoot should not be null');
    const crIconButton = button.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!crIconButton, 'cr-icon-button should not be null');

    // Simulate click
    crIconButton.click();

    const args =
        await browserProxy.toolbarUIHandler.whenCalled('showContextMenu');
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
    assertEquals(ContextMenuType.kBatterySaver, args.menuType);
  });

  test('ShowsTooltip', () => {
    const crIconButton = button.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!crIconButton);
    // Tooltip and aria-label strings should be set
    assertEquals('Energy Saver is on', crIconButton.title);
    assertEquals('Energy Saver is on', crIconButton.getAttribute('aria-label'));
  });

  test('TabindexIsZero', () => {
    const crIconButton = button.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!crIconButton);
    assertEquals('0', crIconButton.getAttribute('tabindex'));
  });
});
