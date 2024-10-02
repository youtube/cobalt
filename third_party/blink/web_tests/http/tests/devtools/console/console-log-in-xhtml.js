// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that console message from inline script in xhtml document contains correct script position information.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.navigatePromise('resources/console-log-in-xhtml.xhtml');

  await ConsoleTestRunner.dumpConsoleMessages(undefined, undefined, simpleFormatter);
  TestRunner.completeTest();
  function simpleFormatter(element, message) {
    return message.messageText + ':' + message.line + ':' + message.column;
  }
})();
