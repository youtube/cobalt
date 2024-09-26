// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that selector line is computed correctly regardless of its start column. Bug 110732.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <style>
      #inspected
      {
        color: green;
      }
      </style>

      <div id="container">
          <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/selector-line.css');

  SourcesTestRunner.waitForScriptSource('selector-line.scss', onSourceMapLoaded);

  function onSourceMapLoaded() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  }

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
