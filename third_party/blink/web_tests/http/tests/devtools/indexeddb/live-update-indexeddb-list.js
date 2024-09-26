// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the IndexedDB database list live updates.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  let indexedDBModel = TestRunner.mainTarget.model(Resources.IndexedDBModel);
  indexedDBModel._throttler._timeout = 0;

  ApplicationTestRunner.dumpIndexedDBTree();
  let promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, '_addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database1');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, '_addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database2');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore1', 'index1');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore2', 'index2');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.createObjectStoreIndexAsync('database1', 'objectStore1', 'index3');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, '_indexRemoved');
  await ApplicationTestRunner.deleteObjectStoreIndexAsync('database1', 'objectStore1', 'index3');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.deleteObjectStoreAsync('database1', 'objectStore2');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, 'setExpandable');
  await ApplicationTestRunner.deleteDatabaseAsync('database1');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, 'setExpandable');
  await ApplicationTestRunner.deleteDatabaseAsync('database2');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();
  TestRunner.completeTest();
})();
