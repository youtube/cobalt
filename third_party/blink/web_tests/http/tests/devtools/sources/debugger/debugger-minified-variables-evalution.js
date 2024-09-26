// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests evaluation in minified scripts.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-expressions-compressed.js');

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              Sources.CallStackSidebarPane.prototype, 'updatedForTest', step2)
  }

  function step2() {
    SourcesTestRunner.waitForScriptSource('resolve-expressions-origin.js', step3);
  }

  function step3(uiSourceCode) {
    var positions = [
      new Position(7, 11, 23, 'object.prop1'), new Position(4, 4, 14, 'this.prop2'),
      new Position(5, 4, 19, 'object["prop3"]'), new Position(2, 8, 14, 'object'),  //object
    ];
    var promise = Promise.resolve();
    for (var position of positions)
      promise = promise.then(testAtPosition.bind(null, uiSourceCode, position));

    promise.then(() => SourcesTestRunner.completeDebuggerTest());
  }

  function Position(line, startColumn, endColumn, originText) {
    this.line = line;
    this.startColumn = startColumn;
    this.endColumn = endColumn;
    this.originText = originText;
  }

  function testAtPosition(uiSourceCode, position) {
    return Sources.SourceMapNamesResolver
        .resolveExpression(
            UI.context.flavor(SDK.DebuggerModel.CallFrame), position.originText, uiSourceCode, position.line,
            position.startColumn, position.endColumn)
        .then(SourcesTestRunner.evaluateOnCurrentCallFrame)
        .then(result => TestRunner.addResult(result.object.description));
  }
})();
