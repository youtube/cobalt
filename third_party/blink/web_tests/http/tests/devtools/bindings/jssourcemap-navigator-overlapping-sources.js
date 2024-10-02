// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult(`Verify that JavaScript SourceMap handle different sourcemaps with overlapping sources.`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/jssourcemaps-with-overlapping-sources/frame1.html', '_test_create-frame1.js'),
    BindingsTestRunner.waitForSourceMap('script1.js.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachAnotherFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('anotherFrame1', './resources/jssourcemaps-with-overlapping-sources/frame1.html', '_test_create-anotherFrame1.js'),
    BindingsTestRunner.waitForSourceMap('script1.js.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame2');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame2', './resources/jssourcemaps-with-overlapping-sources/frame2.html', '_test_create-frame2.js'),
    BindingsTestRunner.waitForSourceMap('script2.js.map')
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachAnotherFrame1');
  await BindingsTestRunner.detachFrame('anotherFrame1', '_test_detach-anotherFrame1.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
