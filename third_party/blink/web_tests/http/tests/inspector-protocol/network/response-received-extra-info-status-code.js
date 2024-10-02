(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the IP address space is reported on responseReceivedExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  session.evaluateAsync(`fetch('index.html');`);
  let event = await dp.Network.onceResponseReceivedExtraInfo();
  testRunner.log(event.params.statusCode);
  session.evaluateAsync(
      `fetch('/inspector-protocol/network/resources/hello-world.html');`);
  event = await dp.Network.onceResponseReceivedExtraInfo();
  testRunner.log(event.params.statusCode);
  testRunner.completeTest();
})
