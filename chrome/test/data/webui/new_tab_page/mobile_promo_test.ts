// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {MobilePromoElement} from 'chrome://new-tab-page/lazy_load.js';
import type {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('MobilePromoTest', () => {
  let newTabPageHandler: TestMock<PageHandlerRemote>;
  let mobilePromo: MobilePromoElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    newTabPageHandler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    void newTabPageHandler;
  });

  async function createMobilePromo(hasQrCode: boolean) {
    if (hasQrCode) {
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
    } else {
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: ''}));
    }
    mobilePromo = document.createElement('ntp-mobile-promo');
    document.body.appendChild(mobilePromo);

    await eventToPromise('qr-code-changed', mobilePromo);
    await microtasksFinished();
  }

  async function createMobilePromoWithQrCode() {
    await createMobilePromo(/*hasQrCode=*/ true);
  }

  async function clickDismissPromoButton(mobilePromo: MobilePromoElement) {
    const parts = mobilePromo.$.titleAndDismissContainer.children;
    assertEquals(2, parts.length);
    const dismissPromoButton = parts[1] as HTMLElement;
    dismissPromoButton.click();
    await microtasksFinished();
  }

  function assertHasQrCode(
      hasQrCode: boolean, mobilePromo: MobilePromoElement) {
    const image = $$<CrAutoImgElement>(mobilePromo, '#qrCode');
    assertTrue(!!image);
    assertEquals(hasQrCode, !image.hidden);
    if (hasQrCode) {
      assertTrue(isVisible(image));
      assertDeepEquals('data:image/webp;base64,abc', image.src);
    } else {
      assertFalse(isVisible(image));
      assertDeepEquals('data:image/webp;base64,', image.src);
    }
  }

  test('render hasQrCode=true', async () => {
    const hasQrCode = true;
    await createMobilePromo(hasQrCode);
    assertHasQrCode(hasQrCode, mobilePromo);
  });

  test('render hasQrCode=false', async () => {
    const hasQrCode = false;
    await createMobilePromo(hasQrCode);
    assertHasQrCode(hasQrCode, mobilePromo);
  });

  test('mobile promo dismisses when dismiss is clicked', async () => {
    await createMobilePromoWithQrCode();
    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);

    await clickDismissPromoButton(mobilePromo);

    assertFalse(isVisible(mobilePromo.$.promoContainer));
    assertTrue(mobilePromo.$.dismissPromoButtonToast.open);
    assertEquals(1, newTabPageHandler.getCallCount('onDismissMobilePromo'));
  });

  test('mobile promo dismissal can be undone', async () => {
    // Dismiss mobile promo.
    await createMobilePromoWithQrCode();
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);
    await clickDismissPromoButton(mobilePromo);
    assertFalse(isVisible(mobilePromo.$.promoContainer));
    assertTrue(mobilePromo.$.dismissPromoButtonToast.open);
    assertEquals(0, newTabPageHandler.getCallCount('onUndoDismissMobilePromo'));

    // Undo dismissal via toast.
    mobilePromo.$.undoDismissPromoButton.click();
    await microtasksFinished();

    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);
    assertEquals(1, newTabPageHandler.getCallCount('onUndoDismissMobilePromo'));
  });

  test('restores promo if undo command is fired via keyboard', async () => {
    // Dismiss mobile promo.
    await createMobilePromoWithQrCode();
    await clickDismissPromoButton(mobilePromo);
    assertEquals(0, newTabPageHandler.getCallCount('onUndoDismissMobilePromo'));

    // Simulate 'ctrl+z' key combo (or meta+z for Mac).
    keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');
    await microtasksFinished();

    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);
    assertEquals(1, newTabPageHandler.getCallCount('onUndoDismissMobilePromo'));
  });

  test('ignores undo command if no promo was blocklisted', async () => {
    await createMobilePromoWithQrCode();

    // Simulate 'ctrl+z' key combo (or meta+z for Mac).
    keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

    assertEquals(0, newTabPageHandler.getCallCount('onUndoDismissMobilePromo'));
  });
});
