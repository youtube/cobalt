// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that simple evaluations may be performed in the console on global object.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var foo = 'fooValue';
    var bar = {
        a: 'b'
    };
  `);

  ConsoleTestRunner.evaluateInConsole('foo', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole('bar', step2);
  }

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
