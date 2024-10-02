// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Checks that script evaluated twice with different source and the same sourceURL won't be diverged from VM.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  const scriptSource = '239\n//# sourceURL=test.js';
  const changedScriptSource = '42\n//# sourceURL=test.js';

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.waitForScriptSource('test.js', step2);
    TestRunner.evaluateInPage(scriptSource);
  }

  function step2(uiSourceCode) {
    TestRunner.addSnifferPromise(Bindings.ResourceScriptFile.prototype, 'mappingCheckedForTest')
        .then(() => step3(uiSourceCode));
    SourcesTestRunner.showScriptSource('test.js');
  }

  function step3(uiSourceCode) {
    var debuggerModel = TestRunner.debuggerModel;
    var scriptFile = Bindings.debuggerWorkspaceBinding.scriptFile(uiSourceCode, debuggerModel);
    if (!scriptFile) {
      TestRunner.addResult('[FAIL]: no script file for test.js');
      SourcesTestRunner.completeDebuggerTest();
      return;
    }
    if (scriptFile.hasDivergedFromVM() || scriptFile.isDivergingFromVM()) {
      TestRunner.addResult('[FAIL]: script file is diverged from VM');
      SourcesTestRunner.completeDebuggerTest();
      return;
    }

    TestRunner
        .addSnifferPromise(
            Sources.DebuggerPlugin.prototype, 'didDivergeFromVM')
        .then(dumpDivergeFromVM);
    TestRunner.addSnifferPromise(Bindings.ResourceScriptFile.prototype, 'mappingCheckedForTest')
        .then(() => SourcesTestRunner.completeDebuggerTest());
    TestRunner.evaluateInPage(changedScriptSource);
  }

  function dumpDivergeFromVM() {
    TestRunner.addResult('[FAIL]: script file was diverged from VM');
  }
})();
