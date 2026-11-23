(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Emulation.setUserAgentOverride overrides User-Agent and Accept-Language headers in WebSocket handshake requests.`);

  await dp.Network.enable();

  // Test User-Agent override.
  await dp.Emulation.setUserAgentOverride({userAgent: 'Test UA 1'});
  session.evaluate(`
    window.ws1 = new WebSocket('ws://localhost:8880/echo');
  `);
  var {request: request1} = (await dp.Network.onceWebSocketWillSendHandshakeRequest()).params;
  testRunner.log('User-Agent: ' + request1.headers['User-Agent']);

  // Test Accept-Language override.
  await dp.Emulation.setUserAgentOverride({userAgent: 'Test UA 2', acceptLanguage: 'ko, en-US'});
  session.evaluate(`
    window.ws2 = new WebSocket('ws://localhost:8880/echo');
  `);
  var {request: request2} = (await dp.Network.onceWebSocketWillSendHandshakeRequest()).params;
  testRunner.log('User-Agent: ' + request2.headers['User-Agent']);
  testRunner.log('Accept-Language: ' + request2.headers['Accept-Language']);

  testRunner.completeTest();
})
