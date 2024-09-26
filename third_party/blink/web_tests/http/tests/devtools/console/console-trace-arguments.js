// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console.trace dumps arguments alongside the stack trace.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function a()
    {
      console.trace(1, 2, 3);
    }
  `);

  async function callback() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
  TestRunner.evaluateInPage('setTimeout(a, 0)');
  ConsoleTestRunner.addConsoleSniffer(callback);
})();
