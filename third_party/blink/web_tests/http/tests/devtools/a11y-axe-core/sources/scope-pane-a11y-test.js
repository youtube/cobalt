// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await UI.viewManager.showView('sources.scopeChain');

  TestRunner.addResult('Testing accessibility in the scope pane.\n');
  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.evaluateInPagePromise(`
  function makeClosure() {
    let x = 5;
    function logX() {
      console.log(x);
      debugger;
    }
    return logX;
  }
  makeClosure()();
  `);
  await SourcesTestRunner.waitUntilPausedPromise();

  await TestRunner.addSnifferPromise(Sources.ScopeChainSidebarPane.prototype, 'sidebarPaneUpdatedForTest');
  const scopePane = Sources.ScopeChainSidebarPane.instance();
  await TestRunner.addSnifferPromise(ObjectUI.ObjectPropertyTreeElement, 'populateWithProperties');
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  TestRunner.addResult('Expanding the makeClosure closure.');
  scopePane.treeOutline.rootElement().childAt(1).expand();
  await TestRunner.addSnifferPromise(ObjectUI.ObjectPropertyTreeElement, 'populateWithProperties');
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  SourcesTestRunner.completeDebuggerTest();
})();
