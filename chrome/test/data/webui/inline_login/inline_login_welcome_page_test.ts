// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {AccountAdditionOptions} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import {InlineLoginAppElement, View} from 'chrome://chrome-signin/inline_login_app.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {fakeAuthExtensionData, fakeAuthExtensionDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

const inline_login_welcome_page_test = {
  suiteName: 'InlineLoginWelcomePageTest',
  TestNames: {
    Reauthentication: 'Reauthentication',
    OkButton: 'OkButton',
    Checkbox: 'Checkbox',
    GoBack: 'GoBack',
    IsAvailableInArc: 'IsAvailableInArc',
    ToggleHidden: 'ToggleHidden',
    LinkClick: 'LinkClick',
  },
};
Object.assign(window, {inline_login_welcome_page_test});

suite(inline_login_welcome_page_test.suiteName, () => {
  let inlineLoginComponent: InlineLoginAppElement;
  let testBrowserProxy: TestInlineLoginBrowserProxy;
  let testAuthenticator: TestAuthenticator;

  /** @return The id of the active view. */
  function getActiveViewId(): string {
    return inlineLoginComponent.shadowRoot!
        .querySelector('div.active[slot="view"]')!.id;
  }

  function testSetup(dialogArgs: AccountAdditionOptions|null) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inlineLoginComponent = document.createElement('inline-login-app');
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthExtHostForTest(testAuthenticator);
    flush();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({shouldSkipWelcomePage: false});
  });

  test(
      inline_login_welcome_page_test.TestNames.Reauthentication, () => {
        testSetup(/*dialogArgs=*/ null);
        webUIListenerCallback(
            'load-auth-extension', fakeAuthExtensionDataWithEmail);
        // Welcome screen should be skipped for reauth.
        assertEquals(
            View.ADD_ACCOUNT, getActiveViewId(),
            'Welcome screen should be active');
      });

  test(inline_login_welcome_page_test.TestNames.OkButton, () => {
    testSetup(/*dialogArgs=*/ null);
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const okButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.next-button');
    assertTrue(!!okButton);
    // OK button and welcome screen should be visible.
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        View.WELCOME, getActiveViewId(), 'Welcome screen should be active');

    okButton.click();
    assertEquals(
        View.ADD_ACCOUNT, getActiveViewId(),
        'Add account screen should be active');

    if (loadTimeData.getBoolean('isArcAccountRestrictionsEnabled')) {
      return;
    }

    return testBrowserProxy.whenCalled('skipWelcomePage').then(skip => {
      assertEquals(
          false, skip, 'skipWelcomePage should be called with "false"');
    });
  });

  test(inline_login_welcome_page_test.TestNames.Checkbox, () => {
    testSetup(/*dialogArgs=*/ null);

    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const welcomePageApp =
        inlineLoginComponent.shadowRoot!.querySelector('welcome-page-app');
    assertTrue(!!welcomePageApp);
    const checkbox = welcomePageApp.shadowRoot!.querySelector('cr-checkbox');
    assertTrue(!!checkbox);
    assertFalse(checkbox.checked, 'Checkbox should be unchecked by default');
    checkbox.click();
    assertTrue(checkbox.checked, 'Checkbox should be checked');

    const okButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.next-button');
    assertTrue(!!okButton);
    okButton.click();
    return testBrowserProxy.whenCalled('skipWelcomePage').then(skip => {
      assertEquals(true, skip, 'skipWelcomePage should be called with "true"');
    });
  });

  test(inline_login_welcome_page_test.TestNames.GoBack, () => {
    testSetup(/*dialogArgs=*/ null);
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const backButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.back-button');
    assertTrue(!!backButton);
    const okButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.next-button');
    assertTrue(!!okButton);

    assertTrue(backButton.hidden, 'Back button should be hidden');
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        View.WELCOME, getActiveViewId(), 'Welcome screen should be active');

    okButton.click();
    assertTrue(okButton.hidden, 'OK button should be hidden');
    assertFalse(backButton.hidden, 'Back button should be visible');
    assertEquals(
        View.ADD_ACCOUNT, getActiveViewId(),
        'Add account screen should be active');

    backButton.click();
    assertTrue(backButton.hidden, 'Back button should be hidden');
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        View.WELCOME, getActiveViewId(), 'Welcome screen should be active');
  });

  test(
      inline_login_welcome_page_test.TestNames.IsAvailableInArc, () => {
        const dialogArgs = {
          isAvailableInArc: true,
          showArcAvailabilityPicker: false,
        };
        testSetup(dialogArgs);
        const welcomePageApp =
            inlineLoginComponent.shadowRoot!.querySelector('welcome-page-app');
        assertTrue(!!welcomePageApp);
        const toggle = welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
            '.arc-toggle-container');
        assertTrue(!!toggle);
        assertFalse(toggle.hidden, 'ARC toggle should be visible');
        const toggleButton =
            welcomePageApp.shadowRoot!.querySelector('cr-toggle');
        assertTrue(!!toggleButton);
        assertTrue(toggleButton.checked);
        toggleButton.click();
        flush();
        return waitAfterNextRender(toggleButton).then(() => {
          assertFalse(toggleButton.checked);
        });
      });

  test(inline_login_welcome_page_test.TestNames.ToggleHidden, () => {
    const dialogArgs = {
      isAvailableInArc: true,
      showArcAvailabilityPicker: true,
    };
    testSetup(dialogArgs);
    const welcomePageApp =
        inlineLoginComponent.shadowRoot!.querySelector('welcome-page-app');
    assertTrue(!!welcomePageApp);
    const toggle = welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
        '.arc-toggle-container');
    assertTrue(!!toggle);
    assertTrue(toggle.hidden, 'ARC toggle should be hidden');
  });

  test(inline_login_welcome_page_test.TestNames.LinkClick, async () => {
    const dialogArgs = {
      isAvailableInArc: true,
      showArcAvailabilityPicker: false,
    };
    testSetup(dialogArgs);

    const welcomePageApp =
        inlineLoginComponent.shadowRoot!.querySelector('welcome-page-app');
    assertTrue(!!welcomePageApp);

    welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
                                  '#osSettingsLink')!.click();
    await testBrowserProxy.whenCalled('dialogClose');

    welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
                                  '#appsSettingsLink')!.click();
    await testBrowserProxy.whenCalled('dialogClose');

    welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
                                  '#newPersonLink')!.click();
    await testBrowserProxy.whenCalled('dialogClose');

    welcomePageApp.shadowRoot!.querySelector<HTMLElement>(
                                  '#guestModeLink')!.click();
    return testBrowserProxy.whenCalled('openGuestWindow');
  });
});
