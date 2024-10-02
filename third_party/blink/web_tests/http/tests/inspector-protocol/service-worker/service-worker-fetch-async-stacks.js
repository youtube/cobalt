(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Async stack trace for service worker fetch.');

  let swdp;
  let swDebuggerId;

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const onAttached = async event => {
    swdp = session.createChild(event.params.sessionId).protocol;
    const idCallback = swdp.Debugger.enable();
    const stackCallback = swdp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    swdp.Runtime.runIfWaitingForDebugger();
    swDebuggerId = (await idCallback).result.debuggerId;
    await stackCallback;
  };
  dp.Target.onAttachedToTarget(onAttached);

  await dp.ServiceWorker.enable();
  await session.navigate('resources/service-worker-fetch.html');
  testRunner.log('Navigated, registering service worker');
  session.evaluateAsync(`navigator.serviceWorker.register('service-worker-fetch.js')`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
    return versions[0].registrationId;
  }

  await waitForServiceWorkerActivation();
  testRunner.log('\nService worker activated, reloading');
  dp.Page.reload();
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');
  testRunner.log('\nReloaded, continue');

  const pageDebuggerId = (await dp.Debugger.enable()).result.debuggerId;
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  await dp.Network.enable();
  testRunner.log(await dp.Network.setAttachDebugStack({enabled: true}), 'enable debug header: ');

  const code = `
      debugger;
      fetch("foo.json");
      //# sourceURL=test.js`;
  testRunner.log('\nEvaluate:\n' + code);
  session.evaluate(code);

  await dp.Debugger.oncePaused();
  testRunner.log('\nPaused in page, step over');
  dp.Debugger.stepOver();

  await dp.Debugger.oncePaused();
  testRunner.log('Run stepInto with breakOnAsyncCall flag');
  dp.Debugger.stepInto({breakOnAsyncCall: true});

  const {callFrames, asyncStackTrace, asyncStackTraceId} = (await swdp.Debugger.oncePaused()).params;
  testRunner.log('\nPaused in service worker');
  await testRunner.logStackTrace(
      new Map([[swDebuggerId, swdp.Debugger], [pageDebuggerId, dp.Debugger]]),
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      swDebuggerId);
  testRunner.completeTest();
})
