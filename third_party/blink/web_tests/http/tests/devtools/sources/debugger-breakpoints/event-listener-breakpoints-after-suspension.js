// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests event listener breakpoints.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadLegacyModule('panels/browser_debugger'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" id="test">
    `);
  await TestRunner.evaluateInPagePromise(`
      function testElementClicked()
      {
          return 0;
      }

      function addListenerAndClick()
      {
          var element = document.getElementById("test");
          element.addEventListener("click", testElementClicked, true);
          element.click();
      }
  `);

  SourcesTestRunner.startDebuggerTest(start);

  function start() {
    SourcesTestRunner.setEventListenerBreakpoint('listener:click', true);
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.evaluateInPageWithTimeout('addListenerAndClick()');
  }

  function paused(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
    printEventTargetName(auxData);
    SourcesTestRunner.resumeExecution(suspendAll);
  }

  function suspendAll() {
    TestRunner.addResult('Suspend all targets');
    SDK.targetManager.suspendAllTargets();
    TestRunner.deprecatedRunAfterPendingDispatches(resumeAll);
  }

  function resumeAll() {
    TestRunner.addResult('Resume all targets');
    SDK.targetManager.resumeAllTargets();
    SourcesTestRunner.waitUntilPaused(finish);
    TestRunner.evaluateInPageWithTimeout('addListenerAndClick()');
  }

  function finish() {
    TestRunner.addResult('Successfully paused after suspension and resuming all targets');
    SourcesTestRunner.completeDebuggerTest();
  }

  function printEventTargetName(auxData) {
    var targetName = auxData && auxData.targetName;
    if (targetName)
      TestRunner.addResult('Event target: ' + targetName);
    else
      TestRunner.addResult('FAIL: No event target name received!');
  }
})();
