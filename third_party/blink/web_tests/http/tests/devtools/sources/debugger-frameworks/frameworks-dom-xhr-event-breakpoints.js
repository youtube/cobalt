// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests framework black-boxing on DOM, XHR and Event breakpoints.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="rootElement"></div>
      <input type="button" id="test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function appendElement(parentId, childId)
      {
          var child = document.createElement("div");
          child.id = childId;
          var parent = document.getElementById(parentId);
          Framework.appendChild(parent, child);
      }

      function sendXHR(url)
      {
          Framework.sendXHR(url);
      }

      function addListenerAndClick(stop)
      {
          function testElementClicked()
          {
              return 0;
          }

          var button = document.getElementById("test");
          var remover = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          if (stop)
              debugger;
          button.click();
          remover();

          remover = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover();

          // Test both handlers together.
          var remover1 = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          var remover2 = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover1();
          remover2();
      }

      function addFewBlackboxedListenersAndClick(addNonBlackboxedListener)
      {
          function testElementClicked()
          {
              return 0;
          }
          function inner()
          {
              var button = document.getElementById("test");
              var remover1 = Framework.addEventListener(button, "click", Framework.empty, true);
              var remover2 = Framework.addEventListener(button, "click", Framework.bind(Framework.throwFrameworkException, null, "EXPECTED"), true);
              var remover3 = Framework.addEventListener(button, "click", Framework.bind(Framework.safeRun, null, Framework.empty, Framework.empty, Framework.empty), true);
              var remover4 = function() {};
              if (addNonBlackboxedListener)
                  remover4 = Framework.addEventListener(button, "click", Framework.bind(Framework.safeRun, null, Framework.empty, testElementClicked, Framework.empty), true);
              debugger;
              button.click();
              remover1();
              remover2();
              remover3();
              remover4();
          }
          return inner();
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  SourcesTestRunner.setQuiet(true);

  SourcesTestRunner.runDebuggerTestSuite([
    function testDOMBreakpoint(next) {
      ElementsTestRunner.nodeWithId('rootElement', step1);

      function step1(node) {
        TestRunner.domDebuggerModel.setDOMBreakpoint(
            node, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
        TestRunner.evaluateInPageWithTimeout(
            'appendElement(\'rootElement\', \'childElement\')');
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
      }
    },

    function testXHRBreakpoint(next) {
      SDK.domDebuggerManager.addXHRBreakpoint('foo', true);
      TestRunner.evaluateInPageWithTimeout('sendXHR(\'resources/foo?xhr\')');
      SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
    },

    function testEventListenerBreakpoint(next) {
      SDK.domDebuggerManager
          .resolveEventListenerBreakpoint({eventName: 'listener:click'})
          .setEnabled(true);
      TestRunner.evaluateInPageWithTimeout('addListenerAndClick(false)');
      SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
          [
            'Print',
            'Resume',
            'Print',
            'Resume',
          ],
          next);
    },

    function testSteppingThroughEventListenerBreakpoint(next) {
      TestRunner.evaluateInPageWithTimeout('addListenerAndClick(true)');
      SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
          [
            'StepOver', 'Print',    'StepOver',
            'Print',  // should break at the first "remover()"
            'StepOver', 'StepOver', 'StepOver',
            'Print',  // enter testElementClicked()
            'StepOut',  'StepOver', 'StepOver', 'StepOver', 'StepOver',
            'Print',  // enter testElementClicked()
            'StepOver', 'StepOver', 'StepOver', 'Print',    'Resume',
          ],
          next);
    },

    function testSteppingOutOnEventListenerBreakpoint(next) {
      TestRunner.evaluateInPageWithTimeout('addListenerAndClick(true)');
      SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
          [
            'StepOut',
            'Print',  // should be in testElementClicked()
            'StepOut',
            'StepOut',
            'Print',  // again in testElementClicked()
            'StepOut',
            'Print',
            'Resume',
          ],
          next);
    },

    function testSteppingOutOnEventListenerBreakpointAllBlackboxedButOne(next) {
      TestRunner.evaluateInPageWithTimeout(
          'addFewBlackboxedListenersAndClick(true)');
      SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
          [
            'StepOut',
            'Print',
            'StepOut',
            'Print',
            'StepOut',
            'Print',
            'Resume',
          ],
          next);
    }
  ]);
})();
