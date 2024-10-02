// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console revokes lazily handled promise rejections.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var p = [];

      function createPromises()
      {
          for (var i = 0; i < 3; ++i)
              p.push(Promise.reject(new Error("Handled error")));
      }

      function handleSomeRejections()
      {
          p[0].catch(function() {});
          p[2].catch(function() {});
      }
  `);

  var messageAddedListener = ConsoleTestRunner.wrapListener(messageAdded);
  const consoleModel = SDK.targetManager.primaryPageTarget().model(SDK.ConsoleModel);
  consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, messageAddedListener);
  Console.ConsoleView.instance().setImmediatelyFilterMessagesForTest();
  Common.settings.moduleSetting('consoleGroupSimilar').set(false);
  TestRunner.addResult('Creating promise');
  TestRunner.evaluateInPageWithTimeout('createPromises()');

  var messageNumber = 0;
  async function messageAdded(event) {
    TestRunner.addResult('Message added: ' + event.data.level + ' ' + event.data.type);
    if (++messageNumber < 3)
      return;
    messageNumber = 0;

    consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, messageAddedListener);
    TestRunner.addResult('');

    // Process array as a batch.
    consoleModel.addEventListener(
        SDK.ConsoleModel.Events.MessageUpdated, ConsoleTestRunner.wrapListener(messageUpdated));
    await ConsoleTestRunner.dumpConsoleCounters();
    TestRunner.addResult('');
    TestRunner.addResult('Handling promise');
    TestRunner.evaluateInPageWithTimeout('handleSomeRejections()');
  }

  async function messageUpdated() {
    if (++messageNumber < 2)
      return;
    await ConsoleTestRunner.dumpConsoleCounters();

    // Turn on verbose filter.
    TestRunner.addResult(`\nEnable verbose filter`);
    Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleFilter.allLevelsFilterValue());
    await ConsoleTestRunner.dumpConsoleCounters();

    TestRunner.completeTest();
  }
})();
