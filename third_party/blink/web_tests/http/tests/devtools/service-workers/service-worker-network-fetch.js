// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests fetch in Service Workers.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('network');

  let scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/network-fetch-worker-scope';

  NetworkTestRunner.recordNetwork();

  ApplicationTestRunner.makeFetchInServiceWorker(scope, '../../network/resources/resource.php', {}, fetchCallback);

  function fetchCallback(result) {
    TestRunner.addResult('Fetch in worker result: ' + result);

    const requests = NetworkTestRunner.networkRequests();
    requests.forEach((request) => {
      TestRunner.addResult(request.url());
      TestRunner.addResult('resource.type: ' + request.resourceType());
      TestRunner.addResult('request.failed: ' + !!request.failed);
    });

    TestRunner.completeTest();
  }
})();
