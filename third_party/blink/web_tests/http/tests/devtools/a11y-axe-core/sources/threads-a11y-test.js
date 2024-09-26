// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Testing accessibility in the threads sidebar pane.');

  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);

  await TestRunner.evaluateInPagePromise(`new Worker('../../sources/resources/worker-source.js')`);
  await SourcesTestRunner.waitUntilPausedPromise();
  const sourcesPanel = UI.panels.sources;
  sourcesPanel.showThreadsIfNeeded();

  const threadsSidebarPane = await sourcesPanel.threadsSidebarPane.widget();
  const threadsSidebarElement = threadsSidebarPane.contentElement;
  TestRunner.addResult(`Threads sidebar pane content:\n ${threadsSidebarElement.deepTextContent()}`);
  TestRunner.addResult('Running the axe-core linter on the threads sidebar pane.');
  await AxeCoreTestRunner.runValidation(threadsSidebarElement);
  TestRunner.completeTest();

})();
