// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that pause on exception works.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function throwAnException()
      {
          return unknown_var;
      }

      function handleClick()
      {
          throwAnException();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    SourcesTestRunner.showScriptSource('debugger-pause-on-exception.js', step2);
  }

  function step2() {
    TestRunner.addResult('Script source was shown.');
    TestRunner.evaluateInPage('setTimeout(handleClick, 0)');
    SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(step3);
  }

  function step3() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
