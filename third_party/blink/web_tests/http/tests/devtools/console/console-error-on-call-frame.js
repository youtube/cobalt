// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console.error does not throw exception when executed in console on call frame.\n`);

  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function testFunction()
    {
      var i = 0;
      debugger;
    }
  `);
  await TestRunner.showPanel('sources');

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole('console.error(42)', step3);
  }

  function step3() {
    SourcesTestRunner.resumeExecution(step4);
  }

  function step4() {
    ConsoleTestRunner.expandConsoleMessages(onExpanded);
  }

  async function onExpanded() {
    var result = (await ConsoleTestRunner.dumpConsoleMessagesIntoArray()).join('\n');
    result = result.replace(/integration_test_runner\.js:\d+/g, '<omitted>');
    TestRunner.addResult(result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
