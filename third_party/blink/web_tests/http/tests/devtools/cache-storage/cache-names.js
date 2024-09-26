// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cache names are correctly loaded and displayed in the inspector.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel);
  cacheStorageModel.enable();

  function errorAndExit(error) {
    if (error)
      TestRunner.addResult(error);
    TestRunner.completeTest();
  }

  function main() {
    ApplicationTestRunner.clearAllCaches()
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache1'))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache2'))
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache3'))
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache4'))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.clearAllCaches)
        .then(TestRunner.completeTest)
        .catch(errorAndExit);
  }

  ApplicationTestRunner.waitForCacheRefresh(main);
})();
