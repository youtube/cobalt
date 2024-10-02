// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('EditPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('password displayed correctly', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(password.username, dialog.$.usernameInput.value);
    assertEquals(password.password, dialog.$.passwordInput.value);
    assertEquals('password', dialog.$.passwordInput.type);

    const listItemElements =
        dialog.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.site-link');
    assertEquals(listItemElements.length, password.affiliatedDomains.length);

    password.affiliatedDomains.forEach((expectedDomain, i) => {
      const listItemElement = listItemElements[i];

      assertTrue(!!listItemElement);
      assertEquals(expectedDomain.name, listItemElement.textContent!.trim());
      assertEquals(expectedDomain.url, listItemElement.href);
    });
  });

  test('show/hide password', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(
        loadTimeData.getString('showPassword'),
        dialog.$.showPasswordButton.title);
    assertEquals('password', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility', dialog.$.showPasswordButton.getAttribute('class'));

    dialog.$.showPasswordButton.click();

    assertEquals(
        loadTimeData.getString('hidePassword'),
        dialog.$.showPasswordButton.title);
    assertEquals('text', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility-off',
        dialog.$.showPasswordButton.getAttribute('class'));
  });

  test('username validation works', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry(
          {url: 'www.example.com', username: 'username1', password: 'pass'}),
      createPasswordEntry(
          {url: 'www.example2.com', username: 'username2', password: 'pass'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains = [
      createAffiliatedDomain('www.example.com'),
      createAffiliatedDomain('www.example2.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = passwordManager.data.passwords[1]!;
    document.body.appendChild(dialog);
    await flushTasks();

    // Update username to the same value as other credential and observe error.
    dialog.$.usernameInput.value = 'username1';
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);
  });

  test('view duplicated password', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry(
          {url: 'www.example.com', username: 'test', password: 'pass'}),
      createPasswordEntry(
          {url: 'www.example2.com', username: 'test2', password: 'pass'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains = [
      createAffiliatedDomain('www.example.com'),
      createAffiliatedDomain('www.example2.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = passwordManager.data.passwords[1]!;
    document.body.appendChild(dialog);
    await flushTasks();

    // Update username to the same value as other credential and observe error.
    dialog.$.usernameInput.value = 'test';
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);

    assertTrue(dialog.$.viewExistingPasswordLink.hidden);
    dialog.showRedirect = true;

    assertFalse(dialog.$.viewExistingPasswordLink.hidden);

    dialog.$.viewExistingPasswordLink.click();
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals('www.example.com', Router.getInstance().currentRoute.details);
  });

  test('note validation works', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.passwordNote.invalid);

    // Make note 899 characters long.
    dialog.$.passwordNote.value = '.'.repeat(899);
    assertFalse(dialog.$.passwordNote.invalid);
    assertEquals('', dialog.$.passwordNote.firstFooter);
    assertEquals('', dialog.$.passwordNote.secondFooter);

    // After 900 characters there are footers.
    dialog.$.passwordNote.value = '.'.repeat(900);
    await flushTasks();
    assertFalse(dialog.$.passwordNote.invalid);
    assertEquals(
        dialog.i18n('passwordNoteCharacterCountWarning', 1000),
        dialog.$.passwordNote.firstFooter);
    assertEquals(
        dialog.i18n('passwordNoteCharacterCount', 900, 1000),
        dialog.$.passwordNote.secondFooter);

    // After 1000 characters note is no longer valid.
    dialog.$.passwordNote.value = '.'.repeat(1000);
    await flushTasks();
    assertTrue(dialog.$.passwordNote.invalid);
    assertEquals(
        dialog.i18n('passwordNoteCharacterCountWarning', 1000),
        dialog.$.passwordNote.firstFooter);
    assertEquals(
        dialog.i18n('passwordNoteCharacterCount', 1000, 1000),
        dialog.$.passwordNote.secondFooter);
  });

  test('password is updated', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'username', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    // Enter website
    dialog.$.usernameInput.value = 'username2';
    dialog.$.passwordInput.value = 'sTroNgPA$$wOrD';
    dialog.$.passwordNote.value = 'super secret note.';

    assertFalse(dialog.$.saveButton.disabled);
    dialog.$.saveButton.click();

    const {id, params} =
        await passwordManager.whenCalled('changeSavedPassword');

    assertEquals(password.id, id);
    assertEquals(dialog.$.usernameInput.value, params.username);
    assertEquals(dialog.$.passwordInput.value, params.password);
    assertEquals(dialog.$.passwordNote.value, params.note);
  });
});
