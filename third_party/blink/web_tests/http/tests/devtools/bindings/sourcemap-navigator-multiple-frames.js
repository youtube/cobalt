// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const GC = async () => {
  await TestRunner.evaluateInPageAsync(`new Promise(resolve =>
    GCController.asyncCollectAll(resolve))`);
};

(async function () {
  TestRunner.addResult(`Verify that SourceMap sources are correctly displayed in navigator.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('initialWorkspace');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/sourcemap-frame.html', '_test_create-iframe1.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame2');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame2', './resources/sourcemap-frame.html', '_test_create-iframe2.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
