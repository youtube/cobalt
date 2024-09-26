// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Checks that we show correct location for script evaluated twice.\n');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPageAnonymously('console.log(1);//# sourceURL=a.js');
  await TestRunner.evaluateInPageAnonymously('console.log(2);//# sourceURL=a.js');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
