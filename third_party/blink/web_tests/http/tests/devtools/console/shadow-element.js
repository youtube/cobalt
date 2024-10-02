// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that $0 works with shadow dom.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <div><div><div id="host"></div></div></div>
  `);
  await TestRunner.evaluateInPagePromise(`
        var host = document.querySelector('#host');
        var sr = host.attachShadow({mode: 'open'});
        sr.innerHTML = "<div><div><div id='shadow'><input id='user-agent-host' type='range'></div></div></div>";
    `);

  Common.settingForTest('showUAShadowDOM').set(true);
  ElementsTestRunner.selectNodeWithId('shadow', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsoleAndDump('\'Author shadow element: \' + $0.id', step3);
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('user-agent-host', step4);
  }

  function step4(node) {
    UI.panels.elements.revealAndSelectNode(node.shadowRoots()[0]);
    ConsoleTestRunner.evaluateInConsoleAndDump('\'User agent shadow host: \' + $0.id', step5);
  }

  function step5() {
    TestRunner.completeTest();
  }
})();
