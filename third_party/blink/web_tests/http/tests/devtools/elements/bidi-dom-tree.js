// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel correctly displays DOM tree structure for bi-di pages.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <div title="ویکی‌پدیا:خوش‌آمدید">ویکی‌پدیا:خوش‌آمدید</div>
    `);

  // Warm up highlighter module.
  TestRunner.loadLegacyModule('source_frame').then(function() {
    ElementsTestRunner.expandElementsTree(step1);
  });

  function step1() {
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();
