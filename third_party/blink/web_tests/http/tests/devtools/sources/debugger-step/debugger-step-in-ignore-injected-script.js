// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that stepInto doesn't pause in InjectedScriptSource.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          console.log(123);
          return 239; // stack result should point here
      }
    `);

  SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true)
      .then(() => SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise())
      .then(() => stepIntoPromise())
      .then(() => stepIntoPromise())
      .then((callFrames) => SourcesTestRunner.captureStackTrace(callFrames))
      .then(() => SourcesTestRunner.completeDebuggerTest());

  function stepIntoPromise() {
    var cb;
    var p = new Promise(fullfill => cb = fullfill);
    SourcesTestRunner.stepInto();
    SourcesTestRunner.waitUntilResumed(() => SourcesTestRunner.waitUntilPaused(cb));
    return p;
  }
})();
