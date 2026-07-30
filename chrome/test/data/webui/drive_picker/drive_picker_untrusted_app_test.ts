// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://drive-picker-host/app.js';

import type {DrivePickerHostUntrustedAppElement} from 'chrome-untrusted://drive-picker-host/app.js';
import {BrowserProxyImpl} from 'chrome-untrusted://drive-picker-host/browser_proxy.js';
import type {PageRemote} from 'chrome-untrusted://drive-picker-host/drive_picker_host_untrusted.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

suite('DrivePickerHostUntrustedAppTest', function() {
  let app: DrivePickerHostUntrustedAppElement;

  setup(async function() {
    if (window.trustedTypes) {
      document.body.innerHTML = window.trustedTypes.emptyHTML;
    } else {
      document.body.innerHTML = '';
    }
    app = document.createElement('drive-picker-host-untrusted-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('AppIsAttached', function() {
    assertTrue(app.isConnected);
  });

  test('LoadsConsentKitUrlIframe', async function() {
    const testUrl = 'https://consent.google.com/primitive?hl=en';

    const browserProxy = BrowserProxyImpl.getInstance();
    const callbackRouterRemote: PageRemote =
        browserProxy.callbackRouter.$.bindNewPipeAndPassRemote();

    callbackRouterRemote.loadConsentKitUrl(testUrl);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!app.shadowRoot);
    const container = app.shadowRoot.querySelector('#picker-container');
    assertTrue(!!container);
    if (!container) {
      return;
    }
    const iframe = container.querySelector('iframe');
    assertTrue(!!iframe);
    if (!iframe) {
      return;
    }
    assertEquals(testUrl, iframe.src);
  });
});
