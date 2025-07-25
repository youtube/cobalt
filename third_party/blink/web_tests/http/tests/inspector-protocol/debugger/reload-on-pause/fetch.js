(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Don't crash reloading while paused in a fetch handler`);

  await dp.Debugger.enable();

  session.navigate('http://devtools.test:8000/inspector-protocol/debugger/reload-on-pause/resources/simple-script.php?script=fetch.js');
  await dp.Debugger.oncePaused();
  testRunner.log('Paused on debugger statement.');

  dp.Page.reload();
  await dp.Page.onceLoadEventFired();
  testRunner.log('Successfully reloaded.');

  testRunner.completeTest();
});
