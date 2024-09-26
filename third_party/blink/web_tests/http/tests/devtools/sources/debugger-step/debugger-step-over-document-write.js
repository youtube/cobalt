// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger StepOver will skip inlined scripts created by document.write().\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/debugger-step-over-document-write.html');

  var numberOfStepOver = 3;

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.showScriptSource(
        'debugger-step-over-document-write.html', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 3, '', true);
    SourcesTestRunner.waitUntilPaused(step3);
    TestRunner.reloadPage(completeTest);
  }

  function step3() {
    var actions = ['Print'];  // First pause on breakpoint.
    for (var i = 0; i < numberOfStepOver; ++i)
      actions.push('StepOver', 'Print');
    actions.push('Resume');
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions);
  }

  function completeTest() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
