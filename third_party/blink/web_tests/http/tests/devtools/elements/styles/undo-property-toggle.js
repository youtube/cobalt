// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that disabling style is undone properly.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="font-weight:bold">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  function step1(node) {
    TestRunner.addResult('Before disable');
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    ElementsTestRunner.dumpStyleTreeItem(treeItem, '');

    ElementsTestRunner.toggleStyleProperty('font-weight', false);
    ElementsTestRunner.waitForStyles('container', step2);
  }

  function step2() {
    TestRunner.addResult('After disable');
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    ElementsTestRunner.dumpStyleTreeItem(treeItem, '');

    SDK.domModelUndoStack.undo();
    ElementsTestRunner.waitForStyles('container', step3);
  }

  function step3() {
    TestRunner.addResult('After undo');
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    ElementsTestRunner.dumpStyleTreeItem(treeItem, '');

    SDK.domModelUndoStack.redo();
    ElementsTestRunner.waitForStyles('container', step4);
  }

  function step4() {
    TestRunner.addResult('After redo');
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('font-weight');
    ElementsTestRunner.dumpStyleTreeItem(treeItem, '');
    TestRunner.completeTest();
  }
})();
