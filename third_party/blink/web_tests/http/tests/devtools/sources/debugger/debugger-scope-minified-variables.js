// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests resolving variable names via source maps.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-variable-names-compressed.js');

  SourcesTestRunner.waitForScriptSource('resolve-variable-names-origin.js', onSourceMapLoaded);

  function onSourceMapLoaded() {
    SourcesTestRunner.startDebuggerTest(() => SourcesTestRunner.runTestFunctionAndWaitUntilPaused());
    TestRunner.addSniffer(Sources.SourceMapNamesResolver, '_scopeResolvedForTest', onScopeResolved, true);
  }

  var resolvedScopes = 0;
  function onScopeResolved() {
    if (++resolvedScopes === 2)
      onAllScopesResolved();
  }

  function onAllScopesResolved() {
    SourcesTestRunner.expandScopeVariablesSidebarPane(onSidebarsExpanded);
  }

  function onSidebarsExpanded() {
    TestRunner.addResult('');
    SourcesTestRunner.dumpScopeVariablesSidebarPane();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
