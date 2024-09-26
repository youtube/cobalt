// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-manageemnt-permission-item. */
import 'chrome://resources/cr_components/app_management/permission_item.js';

import {TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {AppManagementPermissionItemElement} from 'chrome://resources/cr_components/app_management/permission_item.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createTestApp, TestAppManagementBrowserProxy} from './app_management_test_support.js';

suite('AppManagementPermissionItemTest', function() {
  let permissionItem: AppManagementPermissionItemElement;
  let testProxy: TestAppManagementBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const app = createTestApp();
    testProxy = new TestAppManagementBrowserProxy();
    BrowserProxy.setInstance(testProxy);

    permissionItem = document.createElement('app-management-permission-item');
    permissionItem.app = app;
    permissionItem.permissionType = 'kLocation';
    document.body.appendChild(permissionItem);
    await waitAfterNextRender(permissionItem);
  });

  test('Toggle permission', async function() {
    assertFalse(getPermissionValueBool(
        permissionItem.app, permissionItem.permissionType));

    permissionItem.click();
    const data = await testProxy.handler.whenCalled('setPermission');
    assertEquals(data[1].value.tristateValue, TriState.kAllow);

    const metricData = await testProxy.whenCalled('recordEnumerationValue');
    assertEquals(metricData[1], AppManagementUserAction.LOCATION_TURNED_ON);
  });
});
