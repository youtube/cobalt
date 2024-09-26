// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that sourcemap sources are mapped event when sourcemap compiled url matches with one of the source urls.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/sourcemap-name-clash/out.js');

  BindingsTestRunner.initializeTestMapping();
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/sourcemap-name-clash/out.js': null});

  Promise
      .all([
        getResourceContent('out.js', Common.resourceTypes.Script),
        getResourceContent('out.js', Common.resourceTypes.SourceMapScript),
      ])
      .then(onResourceContents);

  function onResourceContents(contents) {
    var fs = new BindingsTestRunner.TestFileSystem('/var/www');
    BindingsTestRunner.addFiles(fs, {
      'out.js': {content: contents[0], time: new Date('December 1, 1989')},
      'src/out.js': {content: contents[1], time: new Date('December 1, 1989')}
    });
    fs.reportCreated(onFileSystemCreated);
  }

  function onFileSystemCreated() {
    var automappingTest = new BindingsTestRunner.AutomappingTest(Workspace.workspace);
    automappingTest.waitUntilMappingIsStabilized().then(TestRunner.completeTest.bind(TestRunner));
  }

  function getResourceContent(name, contentType) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    SourcesTestRunner.waitForScriptSource(name, onSource, contentType);
    return promise;

    function onSource(uiSourceCode) {
      uiSourceCode.requestContent().then(({ content, error, isEncoded }) => fulfill(content));
    }
  }
})();
