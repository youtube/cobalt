// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cache view updates when the cache is changed.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel);
  cacheStorageModel.enable();

  await new Promise(resolve => ApplicationTestRunner.waitForCacheRefresh(resolve));
  await ApplicationTestRunner.clearAllCaches();
  await ApplicationTestRunner.dumpCacheTree();
  await ApplicationTestRunner.createCache('testCache1');
  await ApplicationTestRunner.dumpCacheTree();
  var promise = TestRunner.addSnifferPromise(Resources.ServiceWorkerCacheView.prototype, 'updatedForTest');
  await ApplicationTestRunner.addCacheEntry('testCache1', 'http://fake.request.com/1', 'OK');
  await promise;
  TestRunner.addResult('Added entry');
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();
  promise = TestRunner.addSnifferPromise(Resources.ServiceWorkerCacheView.prototype, 'updatedForTest');
  await ApplicationTestRunner.deleteCacheEntry('testCache1', 'http://fake.request.com/1');
  await promise;
  TestRunner.addResult('Deleted entry');
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();
  await ApplicationTestRunner.clearAllCaches();
  TestRunner.completeTest();
})();
