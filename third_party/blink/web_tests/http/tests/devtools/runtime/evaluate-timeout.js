// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  TestRunner.addResult("Test frontend's timeout support.\n");

  const executionContext = UI.context.flavor(SDK.ExecutionContext);
  const regularExpression = '1 + 1';
  const infiniteExpression = 'while (1){}';

  await runtimeTestCase(infiniteExpression, 0);
  await runtimeTestCase(regularExpression);

  let supports = executionContext.runtimeModel.hasSideEffectSupport();
  TestRunner.addResult(`\nDoes the runtime also support side effect checks? ${supports}`);
  TestRunner.addResult(`\nClearing cached side effect support`);
  executionContext.runtimeModel.hasSideEffectSupportInternal = null;

  // Debugger evaluateOnCallFrame test.
  await TestRunner.evaluateInPagePromise(`
    function testFunction()
    {
        debugger;
    }
  `);
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();

  await debuggerTestCase(infiniteExpression, 0);
  await debuggerTestCase(regularExpression);

  supports = executionContext.runtimeModel.hasSideEffectSupport();
  TestRunner.addResult(`Does the runtime also support side effect checks? ${supports}`);

  SourcesTestRunner.completeDebuggerTest();

  async function runtimeTestCase(expression, timeout) {
    TestRunner.addResult(`\nTesting expression ${expression} with timeout: ${timeout}`);
    const result = await executionContext.evaluate({expression, timeout});
    printDetails(result);
  }

  async function debuggerTestCase(expression, timeout) {
    TestRunner.addResult(`\nTesting expression ${expression} with timeout: ${timeout}`);
    const result = await executionContext.debuggerModel.selectedCallFrame().evaluate({expression, timeout});
    printDetails(result);
  }

  function printDetails(result) {
    if (result.error) {
      TestRunner.addResult(`Error: ${result.error}`);
    } else {
      TestRunner.addResult('Result:');
      TestRunner.addResult(`  Description: ${result.object.description}`);
      TestRunner.addResult(`  Value:       ${result.object.value}`);
      TestRunner.addResult(`  Type:        ${result.object.type}`);

    }
  }
})();
