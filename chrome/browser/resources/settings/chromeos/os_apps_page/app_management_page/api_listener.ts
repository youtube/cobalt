// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createInitialState} from 'chrome://resources/cr_components/app_management/util.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {addApp, changeApp, removeApp} from './actions.js';
import {AppManagementBrowserProxy} from './browser_proxy.js';
import {AppManagementStore} from './store.js';

let initialized = false;

async function init() {
  assert(!initialized);

  const {apps: initialApps} =
      await AppManagementBrowserProxy.getInstance().handler.getApps();
  const initialState = createInitialState(initialApps);
  AppManagementStore.getInstance().init(initialState);

  const callbackRouter = AppManagementBrowserProxy.getInstance().callbackRouter;

  callbackRouter.onAppAdded.addListener(onAppAdded);
  callbackRouter.onAppChanged.addListener(onAppChanged);
  callbackRouter.onAppRemoved.addListener(onAppRemoved);

  initialized = true;
}

function dispatch(action: Action): void {
  AppManagementStore.getInstance().dispatch(action);
}

function onAppAdded(app: App): void {
  dispatch(addApp(app));
}

function onAppChanged(app: App): void {
  dispatch(changeApp(app));
}

function onAppRemoved(appId: string): void {
  dispatch(removeApp(appId));
}

init();
