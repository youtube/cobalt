// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that matching selectors are marked properly after new rule creation and selector change.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected"></div>
    `);

  var nodeId;
  var stylesPane;

  TestRunner.runTestSuite([
    function setUp(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function addRule(next) {
      ElementsTestRunner.nodeWithId('inspected', nodeCallback);

      function nodeCallback(node) {
        nodeId = node.id;
        stylesPane = UI.panels.elements.stylesWidget;
        ElementsTestRunner.addNewRule('foo, #inspected, .bar, #inspected', callback);
      }

      async function callback() {
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, false, true);
        next();
      }
    },

    function changeSelector(next) {
      var section = ElementsTestRunner.firstMatchedStyleSection();
      section.startEditingSelector();
      var selectorElement = section.selectorElement;
      selectorElement.textContent = '#inspected, a, hr';
      ElementsTestRunner.waitForSelectorCommitted(callback);
      selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

      async function callback() {
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, false, true);
        next();
      }
    }
  ]);
})();
