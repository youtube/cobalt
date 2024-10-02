// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that link to snippet works.\n`);

  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  TestRunner.addSniffer(
      Workspace.UISourceCode.prototype, 'addMessage', dumpLineMessage, true);

  TestRunner.runTestSuite([
    function testConsoleLogAndReturnMessageLocation(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise('console.log(239);42')
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name1', uiSourceCode))
        .then(() => runSelectedSnippet());
    },

    function testSnippetSyntaxError(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(1)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise('\n }')
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name2', uiSourceCode))
        .then(() => runSelectedSnippet());
    },

    function testConsoleErrorHighlight(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(4)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise(`
console.error(42);
console.error(-0);
console.error(false);
console.error(null)`)
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name3', uiSourceCode))
        .then(() => runSelectedSnippet());
    }
  ]);

  async function createSnippetPromise(content) {
    const projects = Workspace.workspace.projectsForType(Workspace.projectTypes.FileSystem);
    const snippetsProject = projects.find(project => Persistence.FileSystemWorkspaceBinding.fileSystemType(project) === 'snippets');
    const uiSourceCode = await snippetsProject.createFile('');
    uiSourceCode.setContent(content);
    return uiSourceCode;
  }

  function renameSourceCodePromise(newName, uiSourceCode) {
    var callback;
    var promise = new Promise(fullfill => (callback = fullfill));
    uiSourceCode.rename(newName).then(() => callback(uiSourceCode));
    return promise;
  }

  function selectSourceCode(uiSourceCode) {
    return Common.Revealer.reveal(uiSourceCode).then(() => uiSourceCode);
  }

  function dumpLineMessage(message) {
    TestRunner.addResult(`Line Message was added: ${this.url()} ${
        message.level()} '${message.text()}':${message.lineNumber()}:${
        message.columnNumber()}`);
  }

  function runSelectedSnippet() {
    Sources.SourcesPanel.instance().runSnippet();
  }
})();
