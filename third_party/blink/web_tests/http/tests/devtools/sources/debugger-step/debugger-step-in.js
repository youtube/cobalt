// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests "step in" functionality in debugger.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function d()
      {
          var i = 10;
      }

      function testFunction()
      {
          debugger;
          d();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  var stepCount = 0;
  async function step2(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    if (stepCount < 2) {
      TestRunner.addResult('Stepping into...');
      SourcesTestRunner.stepInto();
      SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step2));
    } else
      SourcesTestRunner.completeDebuggerTest();
    stepCount++;
  }
})();
