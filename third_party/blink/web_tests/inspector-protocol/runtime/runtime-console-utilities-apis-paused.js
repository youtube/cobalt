(async function testCommandLine(testRunner) {
  const { session, dp } = await testRunner.startBlank('Test console command line API via Runtime.evaluate when debugger paused');
  const ConsoleTestHelper = await testRunner.loadScript('./resources/console-test-helper.js');

  await dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(({params}) => {
    const cleanedArgs = params.args.map(arg => {
      const newArg = { ...arg };
      // hide previews, since they expose implementation-dependent information
      if (newArg.preview)
        delete newArg.preview;
      return newArg;
    })
    testRunner.log(cleanedArgs);
  });

  await dp.Debugger.enable();
  session.evaluate('debugger;');
  const paused_event = await dp.Debugger.oncePaused();
  const callFrameId = paused_event.params.callFrames[0].callFrameId;

  const consoleHelper = new ConsoleTestHelper(testRunner, dp,
    (expression, options) => dp.Debugger.evaluateOnCallFrame({ expression, ...options, callFrameId})
  );

  await testRunner.runTestSuite([
    consoleHelper.testDir.bind(consoleHelper),
    consoleHelper.test$.bind(consoleHelper),
    consoleHelper.test$$.bind(consoleHelper),
    consoleHelper.testKeys.bind(consoleHelper),
    consoleHelper.testValues.bind(consoleHelper),
    consoleHelper.test$x.bind(consoleHelper),
    consoleHelper.test$_.bind(consoleHelper),
    consoleHelper.test$0to$4.bind(consoleHelper),
    consoleHelper.testGetEventListeners.bind(consoleHelper),
  ]);

  testRunner.completeTest();
})