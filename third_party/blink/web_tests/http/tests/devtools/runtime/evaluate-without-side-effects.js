// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  TestRunner.addResult("Test frontend's side-effect support check for compatibility.\n");

  const executionContext = UI.context.flavor(SDK.ExecutionContext);
  const expressionWithSideEffect = '(async function(){ await 1; })()';
  const expressionWithoutSideEffect = '1 + 1';

  await runtimeTestCase(expressionWithSideEffect, /* throwOnSideEffect */ true);
  await runtimeTestCase(expressionWithSideEffect, /* throwOnSideEffect */ false);
  await runtimeTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ true);
  await runtimeTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ false);

  let supports = executionContext.runtimeModel.hasSideEffectSupport();
  TestRunner.addResult(`Does the runtime support side effect checks? ${supports}`);
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

  await debuggerTestCase(expressionWithSideEffect, /* throwOnSideEffect */ true);
  await debuggerTestCase(expressionWithSideEffect, /* throwOnSideEffect */ false);
  await debuggerTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ true);
  await debuggerTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ false);

  supports = executionContext.runtimeModel.hasSideEffectSupport();
  TestRunner.addResult(`Does the runtime support side effect checks? ${supports}`);

  SourcesTestRunner.completeDebuggerTest();

  async function runtimeTestCase(expression, throwOnSideEffect) {
    TestRunner.addResult(`\nTesting expression ${expression} with throwOnSideEffect ${throwOnSideEffect}`);
    const result = await executionContext.evaluate({expression, throwOnSideEffect});
    printDetails(result);
  }

  async function debuggerTestCase(expression, throwOnSideEffect) {
    TestRunner.addResult(`\nTesting expression ${expression} with throwOnSideEffect ${throwOnSideEffect}`);
    const result = await executionContext.debuggerModel.selectedCallFrame().evaluate({expression, throwOnSideEffect});
    printDetails(result);
  }

  async function printDetails(result) {
    if (result.error) {
      TestRunner.addResult(`FAIL - Error: ${result.error}`);
    } else if (result.exceptionDetails) {
      let exceptionDescription = result.exceptionDetails.exception.description;
      TestRunner.addResult(`Exception: ${exceptionDescription.split("\n")[0]}`);
    } else if (result.object) {
      let objectDescription = result.object.description;
      TestRunner.addResult(`Result: ${objectDescription}`);
    }
  }
})();
