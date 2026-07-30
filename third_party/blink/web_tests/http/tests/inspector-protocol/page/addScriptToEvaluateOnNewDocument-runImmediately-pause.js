(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `<iframe sandbox></iframe>`,
      `Tests that Page.addScriptToEvaluateOnNewDocument with runImmediately ` +
      `tolerates scripts being added while paused inside an injected script.`);

  await dp.Page.enable();
  await dp.Debugger.enable();

  let pauseCount = 0;
  dp.Debugger.onPaused(() => {
    pauseCount++;
    // Register enough additional scripts to grow the underlying storage.
    for (let i = 0; i < 40; i++) {
      dp.Page.addScriptToEvaluateOnNewDocument(
          {source: 'void 0', runImmediately: false});
    }
    dp.Debugger.resume();
  });

  await dp.Page.addScriptToEvaluateOnNewDocument(
      {source: 'debugger', runImmediately: true});

  testRunner.log(`Paused at least once: ${pauseCount > 0}`);
  testRunner.log('PASS: did not crash');
  testRunner.completeTest();
});
