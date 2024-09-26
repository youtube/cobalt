// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that injected script isn't reported to frontend.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function newWorld()
      {
          testRunner.evaluateScriptInIsolatedWorld(239, "console.log(42);\\n//# sourceURL=239.js");
      }
  `);

  TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, step2);
  TestRunner.evaluateInPage('newWorld()\n//# sourceURL=foo.js');

  var expectedScriptParsed = 2;
  function step2(event) {
    var script = event.data;
    TestRunner.addResult(script.sourceURL);
    --expectedScriptParsed;
    if (!expectedScriptParsed)
      TestRunner.completeTest();
  }
})();
