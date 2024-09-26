// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that client can call undo multiple times with non-empty history.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div style="display:none" id="container">
      </div>
    `);

  var containerNode;
  ElementsTestRunner.expandElementsTree(step1);

  function step1(node) {
    containerNode = ElementsTestRunner.expandedNodeWithId('container');
    TestRunner.addResult('========= Original ========');
    ElementsTestRunner.dumpElementsTree(containerNode);
    step2();
  }

  function step2() {
    function callback() {
      TestRunner.addResult('===== Modified element =====');
      ElementsTestRunner.dumpElementsTree(containerNode);
      step3();
    }
    containerNode.setAttribute('', 'foo="bar"', callback);
  }

  async function step3() {
    await SDK.domModelUndoStack.undo();
    TestRunner.addResult('===== Undo 1 =====');
    ElementsTestRunner.dumpElementsTree(containerNode);

    TestRunner.addResult('===== Undo 2 =====');
    ElementsTestRunner.dumpElementsTree(containerNode);
    TestRunner.completeTest();
  }
})();
