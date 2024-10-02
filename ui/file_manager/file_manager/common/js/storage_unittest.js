// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {storage} from './storage.js';
import {waitUntil} from './test_error_reporting.js';

/**
 * Used to store the test listener calls.
 */
let nsCalled = {};
let valuesChanged = {};

function listener(changedValues, namespace) {
  nsCalled[namespace] = true;
  valuesChanged[namespace] = changedValues;
}

export function setUp() {
  storage.onChanged.resetForTesting();

  // Add the test listeners to default onChanged which is a
  // StorageChangeTracker instance.
  storage.onChanged.addListener(listener);

  nsCalled = {};
  valuesChanged = {};
}

/**
 * Tests that changing the storage.local values is propagated to the
 * listeners of onChanged.
 */
export async function testPropagateLocally(done) {
  // Check: That the listener receives the update.
  await storage.local.setAsync({'local key': 'local value'});
  await waitUntil(() => nsCalled['local']);
  assertDeepEquals(
      valuesChanged['local'], {'local key': {newValue: 'local value'}});

  done();
}

/**
 * Tests the value propagates in its original type, window.localStorage always
 * returns in string, we use JSON.parse() to convert back to original type.
 */
export async function testNumberBool(done) {
  // Check: That the listener from `tracker` receives the update.
  await storage.local.setAsync({'number': 33, 'boolean': true});
  await waitUntil(() => nsCalled['local']);
  assertDeepEquals(valuesChanged['local'], {
    'number': {newValue: 33},
    'boolean': {newValue: true},
  });

  done();
}
