// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that changing a property is undone properly.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .container {
        font-weight: bold
      }
      </style>
      <div id="container" class="container"></div>
      <div id="other" class="container"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', step1);

  async function step1() {
    TestRunner.addResult('Initial value');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('font-weight');
    treeItem.applyStyleText('font-weight: normal', false);
    ElementsTestRunner.waitForStyles('container', step2);
  }

  async function step2() {
    TestRunner.addResult('After changing property');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    SDK.domModelUndoStack.undo();
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step3);
  }

  async function step3() {
    TestRunner.addResult('After undo');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    SDK.domModelUndoStack.redo();
    ElementsTestRunner.selectNodeAndWaitForStyles('container', step4);
  }

  async function step4() {
    TestRunner.addResult('After redo');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
