// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that responseReceived is called on NetworkDispatcher for downloads.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
    function loadIFrameWithDownload()
    {
        var iframe = document.createElement("iframe");
        iframe.setAttribute("src", "resources/download.zzz");
        document.body.appendChild(iframe);
    };
  `);

  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'responseReceived', responseReceived);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFailed', loadingFailed);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
  TestRunner.addIframe('resources/download.zzz');

  function responseReceived(event) {
    var request = NetworkTestRunner.networkLog().requestByManagerAndId(
        TestRunner.networkManager, event.requestId);

    if (/download\.zzz/.exec(request.url())) {
      TestRunner.addResult('Received response for download.zzz');
      TestRunner.addResult('SUCCESS');
      TestRunner.completeTest();
    }
  }

  function loadingFinished(event) {
    var request = NetworkTestRunner.networkLog().requestByManagerAndId(
        TestRunner.networkManager, event.requestId);

    if (/download\.zzz/.exec(request.url())) TestRunner.completeTest();
  }

  function loadingFailed(event) {
    var request = NetworkTestRunner.networkLog().requestByManagerAndId(
        TestRunner.networkManager, event.requestId);

    if (/download\.zzz/.exec(request.url())) TestRunner.completeTest();
  }
})();
