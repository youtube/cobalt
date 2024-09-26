// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test is testing the old breakpoint sidebar pane. Make sure to
  // turn off the new breakpoint pane experiment.
  Root.Runtime.experiments.setEnabled('breakpointView', false);
  TestRunner.addResult(`Verify that breakpoints are moved appropriately in case of page reload.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function addFooJS() {
          var script = document.createElement('script');
          script.src = '${TestRunner.url("./resources/foo.js")}';
          document.body.appendChild(script);
      }
  `);

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFooJSFile(fs);

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    function addNetworkFooJS(next) {
      TestRunner.evaluateInPage('addFooJS()');
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(next);
    },

    function setBreakpointInFileSystemUISourceCode(next) {
      TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
          .then(sourceCode => SourcesTestRunner.showUISourceCodePromise(sourceCode))
          .then(onSourceFrame);

      async function onSourceFrame(sourceFrame) {
        await SourcesTestRunner.setBreakpoint(sourceFrame, 0, '', true);
        SourcesTestRunner.waitBreakpointSidebarPane(true).then(dumpBreakpointSidebarPane).then(next);
      }
    },

    async function reloadPageAndDumpBreakpoints(next) {
      await testMapping.removeBinding('foo.js');
      await Promise.all([SourcesTestRunner.waitBreakpointSidebarPane(), TestRunner.reloadPagePromise()]);
      testMapping.addBinding('foo.js');
      dumpBreakpointSidebarPane();
      next();
    },
  ]);

  function dumpBreakpointSidebarPane() {
    var pane = Sources.JavaScriptBreakpointsSidebarPane.instance();
    if (!pane._emptyElement.classList.contains('hidden'))
      return TestRunner.textContentWithLineBreaks(pane._emptyElement);
    var entries = Array.from(pane.contentElement.querySelectorAll('.breakpoint-entry'));
    for (var entry of entries) {
      var uiLocation = Sources.JavaScriptBreakpointsSidebarPane.retrieveLocationForElement(entry)
      TestRunner.addResult('    ' + uiLocation.uiSourceCode.url() + ':' + uiLocation.lineNumber);
    }
  }
})();
