// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests "Offline" checkbox does not crash. crbug.com/746220\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  const scriptURL = 'resources/service-worker-empty.js';
  const scope = 'resources/offline';

  // Register a service worker.
  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  await ApplicationTestRunner.waitForActivated(scope);

  // Switch offline mode on.
  const oldNetwork = SDK.multitargetNetworkManager.networkConditions();
  SDK.multitargetNetworkManager.setNetworkConditions(SDK.NetworkManager.OfflineConditions);

  // Switch offline mode off.
  SDK.multitargetNetworkManager.setNetworkConditions(oldNetwork);

  // The test passes if it doesn't crash.
  TestRunner.completeTest();
})();
