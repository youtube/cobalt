// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';

import {Actions} from '../common/actions';
import {getSchema} from '../common/schema';

import {globals} from './globals';

declare global {
  interface Window {
    m: typeof m;
    getSchema: typeof getSchema;
    globals: typeof globals;
    Actions: typeof Actions;
  }
}

export function registerDebugGlobals() {
  window.getSchema = getSchema;
  window.m = m;
  window.globals = globals;
  window.Actions = Actions;
}
