// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests a handling of a click on the link in a message, which had been shown before its originating script was added.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function loadScript()
    {
        var script = document.createElement('script');
        script.type = "text/javascript";
        script.src = "../resources/source2.js";
        document.body.appendChild(script);
    }
  `);


  var message = new SDK.ConsoleMessage(
      TestRunner.runtimeModel, Protocol.Log.LogEntrySource.JS,
      Protocol.Log.LogEntryLevel.Info, 'hello?',
      {url: 'http://127.0.0.1:8000/devtools/resources/source2.js'});

  const consoleModel = SDK.targetManager.primaryPageTarget().model(SDK.ConsoleModel);
  consoleModel.addMessage(message);
  TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, onScriptAdded);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.evaluateInPage('loadScript()');

  function onScriptAdded(event) {
    if (!event.data.contentURL().endsWith('source2.js'))
      return;

    TestRunner.addResult('script was added');
    var message = Console.ConsoleView.instance().visibleViewMessages[0];
    var anchorElement = message.element().querySelector('.devtools-link');
    anchorElement.click();
  }

  InspectorFrontendHost.openInNewTab = function() {
    TestRunner.addResult('Failure: Open link in new tab!!');
    TestRunner.completeTest();
  };

  UI.inspectorView.tabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, panelChanged);

  function panelChanged() {
    TestRunner.addResult('Panel ' + UI.inspectorView.tabbedPane.currentTab.id + ' was opened');
    TestRunner.completeTest();
  }
})();
