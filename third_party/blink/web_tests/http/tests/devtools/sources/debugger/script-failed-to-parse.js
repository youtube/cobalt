// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that ParsedScriptSource event is raised after compile script with syntax error.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function addScript(url)
      {
          var head = document.getElementsByTagName('head')[0];
          var script = document.createElement('script');
          script.setAttribute('src', url);
          head.appendChild(script);
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([
    function testScriptParsedEvent(next) {
      TestRunner.debuggerModel.addEventListener(
          SDK.DebuggerModel.Events.ParsedScriptSource, scriptFailedToParse);
      TestRunner.evaluateInPage('addScript(\'resources/script-failed-to-parse.js\')');

      function scriptFailedToParse(event) {
        var script = event.data;
        if (script.sourceURL.indexOf('script-failed-to-parse.js') !== -1) {
          TestRunner.addResult('Event with script-failed-to-parse.js received');
          TestRunner.debuggerModel.removeEventListener(
              SDK.DebuggerModel.Events.ParsedScriptSource, scriptFailedToParse);
          next();
        }
      }
    },
  ]);
})();
