// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests framework blackboxing feature with sourcemaps.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../debugger/resources/framework-with-sourcemap.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          return foo(callback);
      }

      function callback(i)
      {
          return i;
      }
  `);

  TestRunner.addSniffer(Bindings.BlackboxManager.prototype, '_patternChangeFinishedForTests', step1);
  var frameworkRegexString = '/framework\\.js$';
  Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  function step1() {
    SourcesTestRunner.startDebuggerTest(step2, true);
  }

  function step2() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step3);
  }

  function step3() {
    var actions = [
      'Print',                          // "debugger" in testFunction()
      'StepInto', 'StepInto', 'Print',  // entered callback(i)
      'StepOut', 'Print'
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step4);
  }

  function step4() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
