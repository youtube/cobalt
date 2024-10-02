// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that WebSocket network requests do not loose focus on frame being received.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  const wsUrl = 'ws://localhost:8880/echo';
  const networkLogView = UI.panels.network.networkLogView;
  const dataGrid = networkLogView.dataGrid;
  await TestRunner.evaluateInPagePromise('ws = new WebSocket(\'' + wsUrl + '\')');
  var websocketRequest = NetworkTestRunner.findRequestsByURLPattern(createPlainTextSearchRegex(wsUrl))[0];
  await NetworkTestRunner.waitForRequestResponse(websocketRequest);
  var node = await NetworkTestRunner.waitForNetworkLogViewNodeForRequest(websocketRequest);
  networkLogView.refresh();
  node.select();
  logSelectedNode();

  TestRunner.addResult('Sending Websocket frame');
  await TestRunner.evaluateInPagePromise('ws.send(\'test\')');
  await NetworkTestRunner.waitForWebsocketFrameReceived(websocketRequest, 'test');
  networkLogView.refresh();
  TestRunner.addResult('Websocket Frame Received');
  logSelectedNode();

  TestRunner.completeTest();

  function logSelectedNode() {
    TestRunner.addResult('Selected Request: ' + (dataGrid.selectedNode && dataGrid.selectedNode.request().url()) || '');
  }
})();
