// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests event listeners output in the Elements sidebar panel when the listeners are added on an element in about:blank page.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe id="myframe"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function setupEventListeners()
      {
          function f() {}
          var frame = document.getElementById("myframe");
          var body = frame.contentDocument.body;
          body.addEventListener("click", f, true);
          var div = frame.contentDocument.createElement("div");
          div.id = "div-in-iframe";
          div.addEventListener("hover", f, {capture: true, once: true});
          body.appendChild(div);
          body.addEventListener("wheel", f, {"passive": true});
      }
  `);

  Common.settingForTest('showEventListenersForAncestors').set(true);
  TestRunner.evaluateInPage('setupEventListeners()', step1);

  function step1() {
    ElementsTestRunner.selectNodeWithId('div-in-iframe', step2);
  }

  function step2() {
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(TestRunner.completeTest.bind(TestRunner));
  }
})();
