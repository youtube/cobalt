// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that SSP maintains focus if changes occur while editing\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <div id="inspected">Inspected Node</div>
  `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');

  var section = ElementsTestRunner.inlineStyleSection();
  const treeElement = section.addNewBlankProperty(0);

  // Flush the pane's throttler and then stall it.
  const originalDoUpdate = () => treeElement.parentPane().doUpdate();
  await treeElement.parentPane().update();

  // Trigger a model change that will schedule a pane update.
  // Once editing begins, we expect any scheduled updates to be suppressed.
  TestRunner.addSniffer(Elements.StylesSidebarPane.prototype, 'doUpdate', onUpdateScheduled);
  treeElement.applyStyleText('color: red');
  treeElement.startEditing();

  TestRunner.addResult('Start editing');
  dumpFocus();

  async function onUpdateScheduled() {
    TestRunner.addResult('Apply-triggered update is ready');
    await originalDoUpdate();

    // If update was not suppressed, rebuilding will have changed focus.
    dumpFocus();
    TestRunner.completeTest();
  }

  function dumpFocus() {
    const element = Platform.DOMUtilities.deepActiveElement(document);
    TestRunner.addResult(`Active element: ${element.tagName}, ${element.className}`);
  }
})();
