// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console evaluation can be performed in an iframe context.Bug 19872.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.addIframe("http://localhost:8000/devtools/resources/console-cd-iframe.html", {
    name: "myIFrame"
  });

  ConsoleTestRunner.changeExecutionContext('myIFrame');
  ConsoleTestRunner.evaluateInConsoleAndDump('foo', finish, true);
  function finish() {
    TestRunner.completeTest();
  }
})();
