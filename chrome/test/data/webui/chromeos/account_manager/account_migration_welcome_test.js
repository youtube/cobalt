// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://account-migration-welcome/account_migration_welcome_app.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://account-migration-welcome/account_manager_browser_proxy.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestAccountManagerBrowserProxy} from './test_account_manager_browser_proxy.js';

suite('AccountMigrationWelcomeTest', () => {
  /** @type {string} */
  const fakeEmail = 'user@example.com';
  /** @type {AccountMigrationWelcomeElement} */
  let element = null;
  /** @type {TestAccountManagerBrowserProxy} */
  let testBrowserProxy = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    testBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstance(testBrowserProxy);
    element = /** @type {AccountMigrationWelcomeElement} */ (
        document.createElement('account-migration-welcome'));
    document.body.appendChild(element);
    element.userEmail_ = fakeEmail;
    flush();
  });

  test('Close dialog when user clicks "cancel" button', () => {
    const cancelButton = element.$['cancel-button'];
    cancelButton.click();
    assertEquals(1, testBrowserProxy.getCallCount('closeDialog'));
  });

  test('Reauthenticate account when user clicks "migrate" button', async () => {
    const migrateButton = element.$['migrate-button'];
    migrateButton.click();

    assertEquals(1, testBrowserProxy.getCallCount('reauthenticateAccount'));
    const email = await testBrowserProxy.whenCalled('reauthenticateAccount');
    assertEquals(fakeEmail, email);
  });
});
