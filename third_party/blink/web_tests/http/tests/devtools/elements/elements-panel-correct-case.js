// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel shows all types of elements in the correct case.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <svg>
          <feComposite/>
      </svg>
      <svg>
          <circle/>
      </svg>

      <DIv></DIv>
      <DIV></DIV>
    `);
  await TestRunner.addStylesheetTag('resources/elements-panel-styles.css');

  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();
