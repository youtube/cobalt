// Test GC of disconnected and stopped source nodes
function testStoppedSourceGC(task, should, options) {
  let {context, nodeName, constructorMethod, numberOfNodes} = options;
  let countEndedNodes = 0;
  let gain = new GainNode(context, {gain: 1e-10});
  gain.connect(context.destination);

  // When |lastSrc0| stops, it will run gc.
  let lastSrc0 = new ConstantSourceNode(context);
  lastSrc0.connect(gain);

  // When |lastSrc1| stops, we'll verify that the number of handlers left
  // is correct.
  let lastSrc1 = new ConstantSourceNode(context);
  lastSrc1.connect(gain);

  lastSrc0.onended = () => {
    asyncGC().then(function () {
      // Have |lastSrc1| stop 10 render quanta from now (pretty arbitrary).
      lastSrc1.stop(
        context.currentTime + 10 * RENDER_QUANTUM_FRAMES / context.sampleRate);
    });
  };

  lastSrc1.onended = () => {
    // All the disconnected sources should have been collected by now.
    should(internals.audioHandlerCount(), 'Number of handlers after GC')
      .beEqualTo(initialCount);
    task.done();
  };

  let initialCount = 0;
  asyncGC().then(function () {
    initialCount = internals.audioHandlerCount();

    // For information only.  This should obviously always pass.
    should(initialCount, 'Number of handlers before test')
        .beEqualTo(initialCount);
    // Create a bunch of sources for testing.  Since we call stop(), these
    // should all get collected, even though they're no longer connected to
    // the destination.
    for (let k = 0; k < numberOfNodes; ++k) {
      let src = constructorMethod();
      src.start();
      src.connect(gain);
      src.onended = () => {
        ++countEndedNodes;
        if (countEndedNodes == numberOfNodes) {
          // For information only
          should(countEndedNodes, `Number of ${nodeName}s tested`)
              .beEqualTo(numberOfNodes);
          lastSrc0.stop();
        }
      };
      // Stop it after about 2 renders
      src.stop(
          context.currentTime + 2 * RENDER_QUANTUM_FRAMES / context.sampleRate);
      src.disconnect();
    }

    // Start the sources
    lastSrc0.start();
    lastSrc1.start();
  });
}
