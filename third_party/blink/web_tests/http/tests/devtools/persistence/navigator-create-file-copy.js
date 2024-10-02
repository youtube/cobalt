// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator's 'Make a copy' works as expected.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'script.js': {content: 'testme'},
  });
  fs.reportCreated(function() {});
  var uiSourceCode = await TestRunner.waitForUISourceCode('script.js');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);
  TestRunner.addResult('BEFORE:\n' + 'file://' + fs.dumpAsText());
  sourcesNavigator.handleContextMenuCreate(uiSourceCode.project(), '', uiSourceCode);
  await TestRunner.waitForUISourceCode('NewFile');
  TestRunner.addResult('\nAFTER:\n' + 'file://' + fs.dumpAsText());
  TestRunner.completeTest();
})();
