// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that uncaught promise rejections happenned in workers are logged into console.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var worker;

      function startWorker()
      {
          worker = new Worker("resources/worker-with-unhandled-promises.js");
          worker.postMessage("");
      }
  `);

  ConsoleTestRunner.addConsoleViewSniffer(checkConsoleMessages, true);
  TestRunner.evaluateInPage('setTimeout(startWorker, 0)');

  function checkConsoleMessages() {
    var count = ConsoleTestRunner.consoleMessagesCount();
    if (count === 2)
      Common.console.showPromise().then(expand);
  }

  function expand() {
    ConsoleTestRunner.expandConsoleMessages(dump);
  }

  async function dump() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
