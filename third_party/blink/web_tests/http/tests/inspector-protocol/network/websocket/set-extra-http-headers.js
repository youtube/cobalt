(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Network.setExtraHTTPHeaders adds headers to WebSocket handshake requests.`);

  await dp.Network.enable();
  await dp.Network.setExtraHTTPHeaders({headers: {"X-DevTools-Test": "Hello, world!"}});
  session.evaluate(`
    window.ws = new WebSocket('ws://localhost:8880/echo');
  `);
  var {request} = (await dp.Network.onceWebSocketWillSendHandshakeRequest()).params;

  var headers = request.headers;
  testRunner.log('X-DevTools-Test: ' + headers['X-DevTools-Test']);
  testRunner.completeTest();
})
