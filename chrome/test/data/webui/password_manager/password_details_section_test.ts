// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordDetailsCardElement, PasswordDetailsSectionElement, PasswordManagerImpl, PasswordViewPageInteractions, Router} from 'chrome://password-manager/password_manager.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createCredentialGroup, createPasswordEntry} from './test_util.js';

suite('PasswordDetailsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('Navigation from passwords section', async function() {
    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    // Simulate navigation from passwords list.
    const group = createCredentialGroup({name: 'test.com'});
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const title = section.shadowRoot!.querySelector('#title');
    assertTrue(!!title);
    assertEquals(group.name, title.textContent!.trim());
  });

  test('Navigating directly', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'test.com');
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [
          createPasswordEntry({id: 0}),
          createPasswordEntry({id: 1}),
        ],
      }),
      createCredentialGroup({name: 'test1.com'}),
      createCredentialGroup({name: 'test2.com'}),
    ];
    // Simulate successful reauth.
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries.slice());

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    assertEquals(
        PasswordViewPageInteractions.CREDENTIAL_REQUESTED_BY_URL,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
    assertArrayEquals(
        [0, 1], await passwordManager.whenCalled('requestCredentialsDetails'));
    await flushTasks();

    const title = section.$.title;
    assertTrue(!!title);
    assertEquals('test.com', title.textContent!.trim());
  });

  test('Navigating directly fails when group is not found', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'test.com');
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('Navigating directly fails when auth failed', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'test.com');
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [
          createPasswordEntry({id: 0}),
          createPasswordEntry({id: 1}),
        ],
      }),
    ];

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    // Since setRequestCredentialsDetailsResponse was not called, auth has
    // failed.
    assertArrayEquals(
        [0, 1], await passwordManager.whenCalled('requestCredentialsDetails'));
    await flushTasks();

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('Clicking back navigates to passwords section', async function() {
    const group = createCredentialGroup({name: 'test.com'});
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    const backButton = section.$.backButton;

    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    backButton.click();
    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('All password entries are displayed', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
        createPasswordEntry({id: 1, username: 'test2'}),
      ],
    });
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    const entries =
        section.shadowRoot!.querySelectorAll<PasswordDetailsCardElement>(
            'password-details-card');
    assertTrue(!!entries.length);
    assertEquals(entries.length, group.entries.length);
    for (let index = 0; index < entries.length; ++index) {
      assertDeepEquals(entries[index]!.password, group.entries[index]);
    }
  });

  test('Details section closes when password deleted', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
      ],
    });
    passwordManager.data.groups = [group];
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);

    // Assert that details section subscribed as a listener.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);

    // Clear groups and invoke password changes.
    passwordManager.data.groups = [];
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await passwordManager.whenCalled('getCredentialGroups');

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('Details section ignores irrelevant updates', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
      ],
    });
    passwordManager.data.groups = [group];
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);

    // Assert that details section subscribed as a listener.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);

    // Invoke password changes to trigger group refresh logic even though
    // nothing has changed.
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await passwordManager.whenCalled('getCredentialGroups');

    // Verify no calls were made to requestCredentialsDetails().
    assertEquals(0, passwordManager.getCallCount('requestCredentialsDetails'));
    // Verify details page is still visible.
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);
  });

  test('Details section updates info when id changed', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
      ],
    });
    passwordManager.data.groups = [group];
    passwordManager.setRequestCredentialsDetailsResponse(group.entries);
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);

    // Assert that details section subscribed as a listener.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);

    // Update id and invoke password changes.
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 1, username: 'test1'}),
      ],
    })];
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await passwordManager.whenCalled('getCredentialGroups');
    await passwordManager.whenCalled('requestCredentialsDetails');

    // Verify details page is still visible.
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);
  });

  test('Url is updated based on new group info', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 1, username: 'test1'}),
        createPasswordEntry({id: 2, username: 'test2'}),
      ],
    });
    passwordManager.data.groups = [group];
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    await new Promise(resolve => setTimeout(resolve));

    assertEquals(
        2,
        section.shadowRoot!.querySelectorAll('password-details-card').length);

    // Assert that details section subscribed as a listener.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);

    // Delete one credential in the group and invoke password changes.
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.de',
      credentials: [
        createPasswordEntry({id: 1, username: 'test1'}),
      ],
    })];
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries);
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await passwordManager.whenCalled('getCredentialGroups');
    await passwordManager.whenCalled('requestCredentialsDetails');
    await flushTasks();

    // Verify details page is still visible and path is matching new group name.
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals(
        '/passwords/test.de', Router.getInstance().currentRoute.path());
    assertEquals(
        1,
        section.shadowRoot!.querySelectorAll('password-details-card').length);
  });

  test('Page closes when auth times out', async function() {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
      ],
    });
    passwordManager.data.groups = [group];
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section = document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    // Assert that details section subscribed as a listener.
    assertTrue(!!passwordManager.listeners.passwordManagerAuthTimeoutListener);

    passwordManager.listeners.passwordManagerAuthTimeoutListener();
    assertEquals(
        PasswordViewPageInteractions.TIMED_OUT_IN_VIEW_PAGE,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
    await flushTasks();

    // Assert that now Passwords page is shown.
    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('Navigating by domain name', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'www.test.com');
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [
          createPasswordEntry({id: 0}),
          createPasswordEntry({id: 1}),
        ],
      }),
    ];
    passwordManager.data.groups[0]!.entries[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.test.com')];
    passwordManager.data.groups[0]!.entries[1]!.affiliatedDomains =
        [createAffiliatedDomain('test app')];

    // Simulate successful reauth.
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries.slice());

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    assertArrayEquals(
        [0, 1], await passwordManager.whenCalled('requestCredentialsDetails'));
    await flushTasks();

    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    const title = section.$.title;
    assertEquals('test.com', title.textContent!.trim());

    const entries =
        section.shadowRoot!.querySelectorAll<PasswordDetailsCardElement>(
            'password-details-card');
    assertEquals(2, entries.length);
  });
});
