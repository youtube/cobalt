// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that slow completions do not interfere with editing styles.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`<div id="inspected">Text</div>`);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  var section = ElementsTestRunner.inlineStyleSection();
  const treeElement = section.addNewBlankProperty(0);
  treeElement.startEditing();
  await TestRunner.addSnifferPromise(UI.TextPrompt.prototype, 'completionsReady');

  treeElement.nameElement.textContent = 'white-space';
  treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Tab'));
  await TestRunner.addSnifferPromise(UI.TextPrompt.prototype, 'completionsReady');

  // Precondition: we have suggestions and a default queryRange
  // Trigger an input. This will change the queryRange to be 'n'
  // Use 'Tab' to force commit editing before new completions arrive.
  // TextPrompt enters a state where it has no currentSuggestion, and must apply textWithCurrentSuggestion.
  const userInput = 'n'
  treeElement.valueElement.textContent = userInput;
  treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent(userInput));
  treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Tab'));
  await TestRunner.addSnifferPromise(UI.TextPrompt.prototype, 'completionsReady');
  dumpFocus();
  ElementsTestRunner.dumpRenderedMatchedStyles();
  TestRunner.completeTest();

  function dumpFocus() {
    const element = Platform.DOMUtilities.deepActiveElement(document);
    TestRunner.addResult(`Active element: ${element.tagName}, ${element.className}`);
  }
})();
