// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that setting selector text can be undone.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
        color: green;
      }
      </style>
      <div id="inspected"></div>
      <div id="other"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    TestRunner.addResult('=== Before selector modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = ElementsTestRunner.firstMatchedStyleSection();
    section.startEditingSelector();
    section.selectorElement.textContent = '#inspected, #other';
    section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step2);
  }

  async function step2() {
    TestRunner.addResult('=== After selector modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    SDK.domModelUndoStack.undo();
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step3);
  }

  async function step3() {
    TestRunner.addResult('=== After undo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    SDK.domModelUndoStack.redo();
    ElementsTestRunner.selectNodeAndWaitForStyles('other', step4);
  }

  async function step4() {
    TestRunner.addResult('=== After redo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
