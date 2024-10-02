// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementStore} from 'chrome://os-settings/chromeos/os_settings.js';
import {App, AppType, ExtensionAppPermissionMessage, OptionalBool, PageHandlerInterface, PageHandlerReceiver, PageHandlerRemote, PageRemote, Permission, PermissionType, PermissionValue, RunOnOsLoginMode, TriState, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {InstallReason, InstallSource} from 'chrome://resources/cr_components/app_management/constants.js';
import {createBoolPermission, createTriStatePermission, getTriStatePermissionValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

type AppConfig = Partial<App>;
type PermissionMap = Partial<Record<PermissionType, Permission>>;

export interface PermissionOption {
  permissionValue: TriState;
  isManaged: boolean;
  value?: PermissionValue;
}

export class FakePageHandler implements PageHandlerInterface {
  static createWebPermissions(
      options?: Partial<Record<PermissionType, PermissionOption>>):
      PermissionMap {
    const permissionTypes = [
      PermissionType.kLocation,
      PermissionType.kNotifications,
      PermissionType.kMicrophone,
      PermissionType.kCamera,
    ];

    const permissions: PermissionMap = {};

    for (const permissionType of permissionTypes) {
      let permissionValue = TriState.kAllow;
      let isManaged = false;

      if (options && options[permissionType]) {
        const opts = options[permissionType]!;
        permissionValue = opts.value ? getTriStatePermissionValue(opts.value) :
                                       permissionValue;
        isManaged = opts.isManaged || isManaged;
      }
      permissions[permissionType] =
          createTriStatePermission(permissionType, permissionValue, isManaged);
    }

    return permissions;
  }

  static createArcPermissions(optIds?: PermissionType[]): PermissionMap {
    const permissionTypes = optIds || [
      PermissionType.kCamera,
      PermissionType.kLocation,
      PermissionType.kMicrophone,
      PermissionType.kNotifications,
      PermissionType.kContacts,
      PermissionType.kStorage,
    ];

    const permissions: PermissionMap = {};

    for (const permissionType of permissionTypes) {
      permissions[permissionType] =
          createBoolPermission(permissionType, true, /*is_managed=*/ false);
    }

    return permissions;
  }

  static createPermissions(appType: AppType): PermissionMap {
    switch (appType) {
      case (AppType.kWeb):
        return FakePageHandler.createWebPermissions();
      case (AppType.kArc):
        return FakePageHandler.createArcPermissions();
      default:
        return {};
    }
  }

  static createApp(id: string, optConfig?: AppConfig): App {
    const app: App = {
      id: id,
      type: AppType.kWeb,
      title: 'App Title',
      description: '',
      version: '5.1',
      size: '9.0MB',
      isPinned: OptionalBool.kFalse,
      isPolicyPinned: OptionalBool.kFalse,
      installReason: InstallReason.kUser,
      permissions: {},
      hideMoreSettings: false,
      hidePinToShelf: false,
      isPreferredApp: false,
      windowMode: WindowMode.kWindow,
      hideWindowMode: false,
      resizeLocked: false,
      hideResizeLocked: true,
      supportedLinks: [],
      runOnOsLogin: undefined,
      fileHandlingState: undefined,
      installSource: InstallSource.kUnknown,
      appSize: '',
      dataSize: '',
      publisherId: '',
    };

    if (optConfig) {
      Object.assign(app, optConfig);
    }

    // Only create default permissions if none were provided in the config.
    if (!optConfig || optConfig.permissions === undefined) {
      app.permissions = FakePageHandler.createPermissions(app.type);
    }

    return app;
  }

  guid: number;
  overlappingAppIds: string[];
  page: PageRemote;
  private apps_: App[];
  private receiver_: PageHandlerReceiver;
  private resolverMap_: Map<string, PromiseResolver<void>>;

  constructor(page: PageRemote) {
    this.receiver_ = new PageHandlerReceiver(this);

    this.guid = 0;
    this.overlappingAppIds = [];
    this.page = page;

    this.apps_ = [];
    this.resolverMap_ = new Map();
    this.resolverMap_.set('setPreferredApp', new PromiseResolver());
    this.resolverMap_.set('getOverlappingPreferredApps', new PromiseResolver());
  }

  private getResolver_(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap_.get(methodName);
    assert(method, `Method '${methodName}' not found.`);
    return method;
  }

  methodCalled(methodName: string): void {
    this.getResolver_(methodName).resolve();
  }

  async whenCalled(methodName: string): Promise<void> {
    await this.getResolver_(methodName).promise;
    // Support sequential calls to whenCalled by replacing the promise.
    this.resolverMap_.set(methodName, new PromiseResolver());
  }

  getRemote(): PageHandlerRemote {
    return this.receiver_.$.bindNewPipeAndPassRemote();
  }

  async flushPipesForTesting(): Promise<void> {
    await this.page.$.flushForTesting();
  }

  async getApps(): Promise<{apps: App[]}> {
    return {apps: this.apps_};
  }

  async getApp(_appId: string): Promise<{app: App}> {
    assertNotReached();
  }

  async getExtensionAppPermissionMessages(_appId: string):
      Promise<{messages: ExtensionAppPermissionMessage[]}> {
    return {messages: []};
  }

  setApps(appList: App[]): void {
    this.apps_ = appList;
  }

  setPinned(appId: string, isPinned: OptionalBool): void {
    const app = AppManagementStore.getInstance().data.apps[appId];
    assert(app);
    const newApp = {...app, isPinned};
    this.page.onAppChanged(newApp);
  }

  setPermission(appId: string, permission: Permission): void {
    const app = AppManagementStore.getInstance().data.apps[appId];
    assert(app);

    // Check that the app had a previous value for the given permission
    assert(app.permissions[permission.permissionType]);

    const newPermissions = {...app.permissions};
    newPermissions[permission.permissionType] = permission;
    const newApp = {...app, permissions: newPermissions};
    this.page.onAppChanged(newApp);
  }

  setResizeLocked(appId: string, resizeLocked: boolean): void {
    const app = AppManagementStore.getInstance().data.apps[appId];
    assert(app);

    const newApp = {...app, resizeLocked};
    this.page.onAppChanged(newApp);
  }

  setHideResizeLocked(appId: string, hideResizeLocked: boolean): void {
    const app = AppManagementStore.getInstance().data.apps[appId];
    assert(app);

    const newApp = {...app, hideResizeLocked};
    this.page.onAppChanged(newApp);
  }

  uninstall(appId: string): void {
    this.page.onAppRemoved(appId);
  }

  setPreferredApp(appId: string, isPreferredApp: boolean): void {
    const app = AppManagementStore.getInstance().data.apps[appId];
    assert(app);

    const newApp = {...app, isPreferredApp};
    this.page.onAppChanged(newApp);
    this.methodCalled('setPreferredApp');
  }

  openNativeSettings(_appId: string): void {}

  setWindowMode(_appId: string, _windowMode: WindowMode): void {
    assertNotReached();
  }

  setRunOnOsLoginMode(_appId: string, _runOnOsLoginMode: RunOnOsLoginMode):
      void {
    assertNotReached();
  }

  setFileHandlingEnabled(_appId: string, _fileHandlingEnabled: boolean): void {
    assertNotReached();
  }

  showDefaultAppAssociationsUi(): void {
    assertNotReached();
  }

  async getOverlappingPreferredApps(_appId: string):
      Promise<{appIds: string[]}> {
    this.methodCalled('getOverlappingPreferredApps');
    if (!this.overlappingAppIds) {
      return {appIds: []};
    }
    return {appIds: this.overlappingAppIds};
  }

  openStorePage(_appId: string): void {}

  async addApp(optId?: string, optConfig?: AppConfig): Promise<App> {
    optId = optId || String(this.guid++);
    const app = FakePageHandler.createApp(optId, optConfig);
    this.page.onAppAdded(app);
    await this.flushPipesForTesting();
    return app;
  }

  /**
   * Takes an app id and an object mapping app fields to the values they
   * should be changed to, and dispatches an action to carry out these
   * changes.
   */
  async changeApp(id: string, changes: AppConfig): Promise<void> {
    this.page.onAppChanged(FakePageHandler.createApp(id, changes));
    await this.flushPipesForTesting();
  }
}
