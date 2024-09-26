
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {UnusedSitePermissions, SiteSettingsPermissionsBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of SiteSettingsPermissionsBrowserProxy. Provides helper
 * methods for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSiteSettingsPermissionsBrowserProxy extends TestBrowserProxy
    implements SiteSettingsPermissionsBrowserProxy {
  private unusedSitePermissions_: UnusedSitePermissions[] = [];

  constructor() {
    super([
      'acknowledgeRevokedUnusedSitePermissionsList',
      'allowPermissionsAgainForUnusedSite',
      'getRevokedUnusedSitePermissionsList',
      'undoAcknowledgeRevokedUnusedSitePermissionsList',
      'undoAllowPermissionsAgainForUnusedSite',
    ]);
  }

  acknowledgeRevokedUnusedSitePermissionsList() {
    this.methodCalled('acknowledgeRevokedUnusedSitePermissionsList');
  }

  allowPermissionsAgainForUnusedSite(origin: string) {
    this.methodCalled('allowPermissionsAgainForUnusedSite', [origin]);
  }

  setUnusedSitePermissions(unusedSitePermissionsList: UnusedSitePermissions[]) {
    this.unusedSitePermissions_ = unusedSitePermissionsList;
  }

  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]> {
    this.methodCalled('getRevokedUnusedSitePermissionsList');
    return Promise.resolve(this.unusedSitePermissions_.slice());
  }

  undoAcknowledgeRevokedUnusedSitePermissionsList(unusedSitePermissionList:
                                                      UnusedSitePermissions[]) {
    this.methodCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList',
        [unusedSitePermissionList]);
  }

  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions) {
    this.methodCalled(
        'undoAllowPermissionsAgainForUnusedSite', [unusedSitePermissions]);
  }
}
