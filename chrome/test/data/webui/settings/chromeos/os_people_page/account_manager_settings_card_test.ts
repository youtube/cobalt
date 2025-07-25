// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {Account, AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {AccountManagerSettingsCardElement, CrIconButtonElement, ParentalControlsBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy, TestAccountManagerBrowserProxyForUnmanagedAccounts} from './test_account_manager_browser_proxy.js';
import {TestParentalControlsBrowserProxy} from './test_parental_controls_browser_proxy.js';

suite('<account-manager-settings-card>', () => {
  let browserProxy: TestAccountManagerBrowserProxy;
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;
  let primaryDeviceAccount: Account|undefined;

  suiteSetup(async () => {
    loadTimeData.overrideValues({isDeviceAccountManaged: true});

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);

    primaryDeviceAccount = (await browserProxy.getAccounts())
                               .find(account => account.isDeviceAccount);
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('managed badge is visible if device account is managed', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManagerSettingsCard.shadowRoot!.querySelector(
        '.device-account-icon .managed-badge');

    assertTrue(isVisible(managedBadge));
  });

  test('account full name is correct and visible', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const accountFullNameEl =
        accountManagerSettingsCard.shadowRoot!.querySelector(
            '#deviceAccountFullName');

    assert(accountFullNameEl);
    assert(primaryDeviceAccount);
    assertTrue(isVisible(accountFullNameEl));
    assertEquals(
        primaryDeviceAccount.fullName, accountFullNameEl.textContent!.trim());
  });

  test('account email is correct and visible', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const accountEmailEl = accountManagerSettingsCard.shadowRoot!.querySelector(
        '#deviceAccountEmail');

    assert(accountEmailEl);
    assert(primaryDeviceAccount);
    assertTrue(isVisible(accountEmailEl));
    assertEquals(
        primaryDeviceAccount.email, accountEmailEl.textContent!.trim());
  });
});

suite('AccountManagerUnmanagedAccountTests', () => {
  let browserProxy: TestAccountManagerBrowserProxyForUnmanagedAccounts;
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({isDeviceAccountManaged: false});

    browserProxy = new TestAccountManagerBrowserProxyForUnmanagedAccounts();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('ManagementStatusForUnmanagedAccounts', async () => {
    await browserProxy.whenCalled('getAccounts');
    flush();

    const managedBadge = accountManagerSettingsCard.shadowRoot!.querySelector(
        '.device-account-icon .managed-badge');
    // Managed badge should not be shown for unmanaged accounts.
    assertNull(managedBadge);
  });
});

suite('AccountManagerAccountChildAccountTests', () => {
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;
  let accountManagerSettingsCard: AccountManagerSettingsCardElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({isChild: true, isDeviceAccountManaged: true});

    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);
  });

  setup(() => {
    accountManagerSettingsCard =
        document.createElement('account-manager-settings-card');
    document.body.appendChild(accountManagerSettingsCard);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    flush();
  });

  teardown(() => {
    accountManagerSettingsCard.remove();
    parentalControlsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('FamilyLinkIcon', () => {
    const icon = accountManagerSettingsCard.shadowRoot!
                     .querySelector<CrIconButtonElement>(
                         '.managed-message cr-icon-button');
    assertTrue(!!icon, 'Could not find the managed icon');

    assertEquals('cr20:kite', icon.ironIcon);

    icon.click();
    assertEquals(
        1,
        parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'));
  });
});
