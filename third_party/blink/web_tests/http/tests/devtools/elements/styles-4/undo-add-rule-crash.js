// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the inspected page does not crash after undoing a new rule addition. Bug 104806\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @keyframes cfpulse1 { 0% { opacity: 0.1;  } }
      .c1 { animation-name: cfpulse1;  }
      </style>
      <div id="inspected"><div id="other"></div>
      <style>
      @keyframes cfpulse1 { 0% { opacity: 0.1;  } }
      .c1 { animation-name: cfpulse1;  }
      </style>

      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    addNewRuleAndSelectNode('other', step2);
  }

  function step2() {
    SDK.domModelUndoStack.undo();
    ElementsTestRunner.waitForStyles('other', step3);
  }

  function step3() {
    TestRunner.completeTest();
  }

  function addNewRuleAndSelectNode(nodeId, next) {
    ElementsTestRunner.addNewRule(null, ruleAdded);

    function ruleAdded() {
      ElementsTestRunner.selectNodeAndWaitForStyles(nodeId, next);
    }
  }
})();
