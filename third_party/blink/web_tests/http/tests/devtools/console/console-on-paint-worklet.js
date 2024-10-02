// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests console output from PaintWorklet.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function importWorklet() {
        CSS.paintWorklet.addModule('resources/console-worklet-script.js');
      }
  `);

  ConsoleTestRunner.waitForConsoleMessages(6, finish);
  TestRunner.evaluateInPage('importWorklet();');

  async function finish() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
