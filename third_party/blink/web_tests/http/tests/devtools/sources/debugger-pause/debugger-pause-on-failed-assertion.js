// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger breaks on failed assertion when pause on exception mode is enabled.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function failAssertion()
      {
          console.assert(false);
      }

      function handleClick()
      {
          failAssertion();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    SourcesTestRunner.showScriptSource(
        'debugger-pause-on-failed-assertion.js', step2);
  }

  function step2() {
    TestRunner.addResult('Script source was shown.');
    TestRunner.evaluateInPage('setTimeout(handleClick, 0)');
    SourcesTestRunner.waitUntilPaused(step3);
  }

  async function step3(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
