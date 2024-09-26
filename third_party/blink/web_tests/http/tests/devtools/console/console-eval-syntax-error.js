// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluating an expression with a syntax error in the console won't crash the browser. Bug 61194.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  ConsoleTestRunner.evaluateInConsole('foo().', step1);

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
