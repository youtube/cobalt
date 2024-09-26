// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console logging dumps proxy properly.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    window.accessedGet = false;
    function testFunction()
    {
      let proxied = new Proxy({ boo: 42 }, {
        get: function (target, name, receiver) {
          window.accessedGet = true;
          return target[name];
        },
        set: function(target, name, value, receiver) {
          target[name] = value;
          return value;
        }
      });
      proxied.foo = 43;
      console.log(proxied);
      let proxied2 = new Proxy(proxied, {});
      console.log(proxied2);
    }
  `);

  ConsoleTestRunner.waitUntilNthMessageReceived(2, dumpMessages);
  TestRunner.evaluateInPage('testFunction()');

  async function dumpMessages() {
    var consoleView = Console.ConsoleView.instance();
    consoleView.viewport.invalidate();
    var element = consoleView.visibleViewMessages[0].contentElement();

    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.evaluateInPage('window.accessedGet', dumpAccessedGetAndExpand);
  }

  function dumpAccessedGetAndExpand(result) {
    TestRunner.addResult('window.accessedGet = ' + result);
    ConsoleTestRunner.expandConsoleMessages(dumpExpandedConsoleMessages);
  }

  async function dumpExpandedConsoleMessages() {
    var element = Console.ConsoleView.instance().visibleViewMessages[0].contentElement();
    dumpNoteVisible(element, 'info-note');

    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.evaluateInPage('window.accessedGet', dumpAccessedGetAndCompleteTest);
  }

  function dumpAccessedGetAndCompleteTest(result) {
    TestRunner.addResult('window.accessedGet = ' + result);
    TestRunner.completeTest();
  }

  function dumpNoteVisible(element, name) {
    var note = window.getComputedStyle(element.querySelector('.object-state-note.' + name)).display;
    TestRunner.addResult(name + ' display: ' + note);
  }
})();
