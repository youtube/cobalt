// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests framework event listeners output in Sources panel when service worker is present.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.showPanel('elements');

  await TestRunner.evaluateInPage(`
    function testFunction() {}
  `);

  await TestRunner.loadHTML(`
      <body onload="testFunction()">
        <button id="inspectedNode">Inspect Me</button>
      </body>
    `);

  Common.settingForTest('showEventListenersForAncestors').set(false);
  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';

  ApplicationTestRunner.waitForServiceWorker(step1);
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);

  var objectEventListenersPane =
      BrowserDebugger.ObjectEventListenersSidebarPane.instance();

  function isServiceWorker() {
    var target = UI.context.flavor(SDK.ExecutionContext).target();
    return target.type() === SDK.Target.Type.ServiceWorker;
  }

  function step1(target) {
    SourcesTestRunner.waitForExecutionContextInTarget(target, step2);
  }

  async function step2(executionContext) {
    TestRunner.addResult('Selecting service worker thread');
    SourcesTestRunner.selectThread(executionContext.target());
    TestRunner.addResult('Context is service worker: ' + isServiceWorker());
    TestRunner.addResult('Dumping listeners');
    await UI.viewManager.showView('sources.globalListeners').then(() => {
      objectEventListenersPane.update();
      ElementsTestRunner.expandAndDumpEventListeners(objectEventListenersPane.eventListenersView, step3);
    });
  }

  function step3() {
    TestRunner.addResult('Selecting main thread');
    SourcesTestRunner.selectThread(SDK.targetManager.primaryPageTarget());
    TestRunner.addResult('Context is service worker: ' + isServiceWorker());
    TestRunner.addResult('Dumping listeners');
    ElementsTestRunner.expandAndDumpEventListeners(objectEventListenersPane.eventListenersView, step4);
  }

  async function step4() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
