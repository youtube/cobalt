// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that removal of one of the multiple projects, all of which are associated with the same frame, doesn't lead navigator to discard the frame treenode.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var rootURL = 'http://localhost:8080/LayoutTests/inspector/debugger/';
  var sourcesNavigatorView = new Sources.NetworkNavigatorView();
  sourcesNavigatorView.show(UI.inspectorView.element);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding urls');
  await SourcesTestRunner.addScriptUISourceCode(rootURL + 'foo/bar/script.js', '', false);
  var contentUISourceCode =
      await SourcesTestRunner.addScriptUISourceCode(rootURL + 'foo/bar/contentScript2.js?a=1', '', true, 42);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Removing contentScripts project');
  contentUISourceCode.project().removeProject();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigatorView);
  TestRunner.completeTest();
})();
