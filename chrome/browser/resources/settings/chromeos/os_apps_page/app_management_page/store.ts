// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

import {Action, Store} from 'chrome://resources/ash/common/store/store.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createEmptyState} from 'chrome://resources/cr_components/app_management/util.js';

import {reduceAction} from './reducers.js';

export type AppMap = Record<string, App>;

export interface AppManagementPageState {
  apps: AppMap;
  selectedAppId: string|null;
}

let instance: AppManagementStore|null = null;

export class AppManagementStore extends Store<AppManagementPageState> {
  static getInstance(): AppManagementStore {
    return instance || (instance = new AppManagementStore());
  }

  static setInstanceForTesting(obj: AppManagementStore): void {
    instance = obj;
  }

  constructor() {
    super(
        createEmptyState(),
        reduceAction as (state: AppManagementPageState, action: Action) =>
            AppManagementPageState);
  }
}
