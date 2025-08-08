(async function testCommandLine(testRunner) {
  const { dp } = await testRunner.startBlank('Test console command line API via Runtime.evaluate.');
  const ConsoleTestHelper = await testRunner.loadScript('./resources/console-test-helper.js');

  const consoleHelper = new ConsoleTestHelper(testRunner, dp,
    (expression, options) => dp.Runtime.evaluate({ expression, ...options })
  );

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