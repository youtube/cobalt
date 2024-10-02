(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that one can evaluate in worker while main page is paused.');

  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  await dp.Debugger.enable();
  const pausedPromise = dp.Debugger.oncePaused();
  dp.Runtime.evaluate({expression: 'debugger;' });
  await pausedPromise;
  testRunner.log(`Paused on 'debugger;'`);

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});

  const messageObject = await attachedPromise;
  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');

  const childSession = session.createChild(messageObject.params.sessionId);
  const result = await childSession.evaluateAsync('1+1');
  testRunner.log('Successfully evaluated, result: ' + result);
  testRunner.completeTest();
})
