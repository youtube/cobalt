// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that there is no javascript error when console evaluation causes resource loading.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  TestRunner.reloadPage(step1);

  function step1() {
    TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestStarted, onRequest);
    var str = '';
    str += 'var s = document.createElement("script");';
    str += 's.src = "resources/silent_script.js";';
    str += 'document.head.appendChild(s);';
    UI.context.flavor(SDK.ExecutionContext).evaluate({expression: str, objectGroup: 'console'});
  }

  function onRequest(event) {
    var request = event.data.request;
    if (/silent_script.js/.test(request.url()))
      step2();
  }

  function step2() {
    var results = NetworkTestRunner.findRequestsByURLPattern(/silent_script.js/);
    if (results.length === 0)
      return;

    TestRunner.completeTest();
  }
})();
