// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluation in console works fine when script is paused. It also checks that stack and global variables are accessible from the console.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var globalVar = { b: 1 };

      function slave(x)
      {
          var y = 20;
          debugger;
      }

      function testFunction()
      {
          var localObject = { a: 300 };
          slave(4000);
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              Sources.CallStackSidebarPane.prototype, 'updatedForTest', step2);
  }

  function step2(callFrames) {
    ConsoleTestRunner.evaluateInConsole('x + y + globalVar.b', step3.bind(null, callFrames));
  }

  function step3(callFrames, result) {
    TestRunner.addResult('Evaluated script on the top frame: ' + result);
    var pane = Sources.CallStackSidebarPane.instance();
    pane.selectNextCallFrameOnStack();
    TestRunner.deprecatedRunAfterPendingDispatches(step4);
  }

  function step4() {
    ConsoleTestRunner.evaluateInConsole('localObject.a + globalVar.b', step5);
  }

  function step5(result) {
    TestRunner.addResult('Evaluated script on the calling frame: ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
