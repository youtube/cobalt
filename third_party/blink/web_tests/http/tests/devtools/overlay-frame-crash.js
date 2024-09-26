// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Regression test for crbug.com/1058718\n');

  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`<div id='foo'>foo</div>`);
  await TestRunner.addScriptTag('network/resources/gc.js');

  ElementsTestRunner.selectNodeWithId('foo', step1);

  function step1(node) {
    TestRunner.evaluateInPage('gc()', step2);
  }

  async function step2() {
    // Pass if it does not crash
    TestRunner.completeTest();
  }
})();
