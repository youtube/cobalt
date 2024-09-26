// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console exposes last evaluation result as $_.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');


  TestRunner.runTestSuite([
    function testLastResult(next) {
      ConsoleTestRunner.evaluateInConsole('1+1', step1);

      function step1() {
        evaluateLastResultAndDump(next);
      }
    },
    function testLastResultAfterConsoleClear(next) {
      ConsoleTestRunner.evaluateInConsole('1+1', step1);

      function step1() {
        Console.ConsoleView.clearConsole();
        TestRunner.deprecatedRunAfterPendingDispatches(step2);
      }

      function step2() {
        evaluateLastResultAndDump(next);
      }
    }
  ]);

  function evaluateLastResultAndDump(callback) {
    ConsoleTestRunner.evaluateInConsole('$_', didEvaluate);

    async function didEvaluate() {
      await ConsoleTestRunner.dumpConsoleMessages();

      if (callback)
        callback();
    }
  }
})();
