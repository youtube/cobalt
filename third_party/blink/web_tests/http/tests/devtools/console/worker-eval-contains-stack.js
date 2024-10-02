// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests exception message from eval on worker context in console contains stack trace.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function startWorker()
      {
          var worker = new Worker("resources/worker.js");
      }
  `);

  TestRunner.addSniffer(SDK.RuntimeModel.prototype, 'executionContextCreated', contextCreated);
  TestRunner.evaluateInPage('startWorker()');

  function contextCreated() {
    ConsoleTestRunner.changeExecutionContext('\u2699 worker.js');
    ConsoleTestRunner.evaluateInConsole('\
            function foo()\n\
            {\n\
                throw {a:239};\n\
            }\n\
            function boo()\n\
            {\n\
                foo();\n\
            }\n\
            boo();', step2);
  }

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(step3);
  }

  async function step3() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
