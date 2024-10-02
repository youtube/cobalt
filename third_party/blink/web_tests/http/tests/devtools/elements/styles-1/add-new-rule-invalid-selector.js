// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that adding a new rule with invalid selector works as expected.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  TestRunner.runTestSuite([
    function init(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function keyframesRuleSelector(next) {
      ElementsTestRunner.addNewRule('@-webkit-keyframes shake', callback);

      async function callback() {
        await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
        next();
      }
    }
  ]);
})();
