// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger StepOut will stop inside next timeout handler.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(callback1, 0);
      }

      function callback1()
      {
          setTimeout(callback2, 0);
          debugger;
          return 1;
      }

      function callback2()
      {
          var dummy = 42; // Should pause here.
          (function FAIL_Should_Not_Pause_Here() { debugger; })();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',  // "debugger" in callback1
      'StepOut',
      'Print',
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
        actions, () => SourcesTestRunner.completeDebuggerTest());
  }
})();
