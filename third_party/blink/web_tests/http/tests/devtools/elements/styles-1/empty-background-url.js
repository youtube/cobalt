// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that empty url in the property value does not break inspector.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        #inspected {
            background-image: url();
        }
      </style>
      <div id="inspected"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('inspected', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false);
    TestRunner.completeTest();
  }
})();
