// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console nicely formats performance getters.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function logToConsole()
    {
        console.log(performance.timing);
        console.log(performance.memory);
    }

    logToConsole();
  `);

  TestRunner.evaluateInPage('logToConsole()', callback);

  async function callback() {
    var messages = await ConsoleTestRunner.dumpConsoleMessagesIntoArray();
    messages.map(m => TestRunner.addResult(m.replace(/:\s+\d+/g, ': <number>')));
    TestRunner.completeTest();
  }
})();
