(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that extra info arrives for the worker main script.`);

  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  await session.evaluateAsync(`
    window.iframe1 = document.createElement('iframe');
    iframe1.src = 'about:blank';
    document.body.appendChild(iframe1);

    window.iframe2 = iframe1.contentWindow.document.createElement('iframe');
    iframe2.src = 'about:blank';
    iframe1.contentWindow.document.body.appendChild(iframe2);
  `);

  const testWorker = async (code, parentSession) => {
    const attachedPromise = parentSession.protocol.Target.onceAttachedToTarget();
    const requestPromise = parentSession.protocol.Network.onceRequestWillBeSent();
    // Extra info events arrive on the page only, similar to request interception for historical reasons.
    const requestExtraPromise = dp.Network.onceRequestWillBeSentExtraInfo();
    const responseExtraPromise = dp.Network.onceResponseReceivedExtraInfo();

    parentSession.evaluateAsync(code);

    const attachedEvent = await attachedPromise;
    testRunner.log('attached to ' + attachedEvent.params.targetInfo.url);
    const workerSession = parentSession.createChild(attachedEvent.params.sessionId);
    const workerDP = workerSession.protocol;
    workerDP.Network.enable();
    workerDP.Runtime.runIfWaitingForDebugger();
    const responsePromise = workerDP.Network.onceResponseReceived();

    const requestEvent = await requestPromise;
    const requestExtraEvent = await requestExtraPromise;
    const responseEvent = await responsePromise;
    const responseExtraEvent = await responseExtraPromise;

    testRunner.log('requestWillBeSent: ' + requestEvent.params.request.url);
    testRunner.log('requestWillBeSentExtraInfo requestId matches: ' + (requestExtraEvent.params.requestId === requestEvent.params.requestId));
    testRunner.log('responseReceived: ' + responseEvent.params.response.url);
    testRunner.log('responseReceived requestId matches: ' + (responseEvent.params.requestId === requestEvent.params.requestId));
    testRunner.log('responseReceivedExtraInfo requestId matches: ' + (responseExtraEvent.params.requestId === requestEvent.params.requestId));

    return workerSession;
  };

  await dp.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  testRunner.log('\nworker in main frame');
  const worker1 = await testWorker(`
    {
      const g = window;
      g.worker = new g.Worker('/inspector-protocol/fetch/resources/worker.js');
    }
  `, session);

  testRunner.log('\nworker in grand-child frame');
  const worker2 = await testWorker(`
    {
      const g = iframe2.contentWindow;
      g.worker = new g.Worker('/inspector-protocol/fetch/resources/worker.js');
    }
  `, session);


  testRunner.log('\nworker in worker in main frame');
  await worker1.protocol.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  await worker1.protocol.Network.enable();
  await worker1.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await testWorker(`
    {
      const g = self;
      g.worker = new g.Worker('/inspector-protocol/fetch/resources/worker.js');
    }
  `, worker1);

  testRunner.log('\nworker in worker in grand-child frame');
  await worker2.protocol.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  await worker2.protocol.Network.enable();
  await worker2.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await testWorker(`
    {
      const g = self;
      g.worker = new g.Worker('/inspector-protocol/fetch/resources/worker.js');
    }
  `, worker2);

  testRunner.completeTest();
})
