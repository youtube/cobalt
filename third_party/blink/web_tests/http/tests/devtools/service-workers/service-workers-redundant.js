// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`ServiceWorkers must be shown correctly even if there is a redundant worker.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('resources');

  const scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/changing-worker.php';
  const scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-redundant-scope/';
  const frameId = 'frame_id';
  let step = 0;
  let firstVersionId = -1;
  let secondVersionId = -1;
  Resources.ServiceWorkersView.setThrottleDisabledForDebugging(true);

  TestRunner.addSniffer(Resources.ServiceWorkersView.prototype, 'updateRegistration', updateRegistration, true);
  function updateRegistration(registration) {
    if (registration.scopeURL != scope)
      return;
    for (let version of registration.versions.values()) {
      if (step == 0 && version.isRunning() && version.isActivated()) {
        ++step;
        firstVersionId = version.id;
        TestRunner.addResult('The first ServiceWorker is activated.');
        TestRunner.addResult('==== ServiceWorkersView ====');
        TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView([scope]));
        TestRunner.addResult('============================');
        TestRunner.addIframe(scope, {id: frameId});
      } else if (step == 1 && version.isRunning() && version.isInstalled()) {
        ++step;
        secondVersionId = version.id;
        TestRunner.addResult('The second Serviceworker is installed.');
        TestRunner.addResult('==== ServiceWorkersView ====');
        TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView([scope]));
        TestRunner.addResult('============================');
        TestRunner.evaluateInPagePromise(`document.getElementById('${frameId}').remove();`);
      }
    }
    if (step != 2)
      return;
    const firstVersion = registration.versions.get(firstVersionId);
    const secondVersion = registration.versions.get(secondVersionId);
    if ((!firstVersion || (firstVersion.isStopped() && firstVersion.isRedundant())) &&
        secondVersion.isActivated() && secondVersion.isRunning()) {
      ++step;
      TestRunner.addResult('The first ServiceWorker worker became redundant and stopped.');
      TestRunner.addResult('==== ServiceWorkersView ====');
      TestRunner.addResult(ApplicationTestRunner.dumpServiceWorkersView([scope]));
      TestRunner.addResult('============================');
      ApplicationTestRunner.deleteServiceWorkerRegistration(scope);
      TestRunner.completeTest();
    }
  }
  UI.panels.resources.sidebar.serviceWorkersTreeElement.select();
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
})();
