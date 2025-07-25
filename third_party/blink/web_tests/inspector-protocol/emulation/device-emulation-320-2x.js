(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that device emulation affects media rules, viewport meta tag, body dimensions and window.screen.');

  var DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  var deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(1200, 1000, 2);

  var viewport = 'w=320';
  testRunner.log(`Loading page with viewport=${viewport}`);
  await session.navigate('../resources/device-emulation.html?' + viewport);

  testRunner.log(await session.evaluate(`dumpMetrics(true)`));
  testRunner.completeTest();
})
