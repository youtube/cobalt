(async function testRemoteObjects(testRunner) {
  const { session, dp } = await testRunner.startBlank('Tests ConsoleOM APIs with unique argument behavior when debugger is paused.');

  const ConsoleTestHelper = await testRunner.loadScript('./resources/console-test-helper.js');

  dp.Runtime.enable();
  dp.DOM.enable();

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const firstParamValue = params.args[0].value;
    const value = params.type === "timeEnd" ? firstParamValue.split(':')[0] : firstParamValue;
    testRunner.log(`${params.type}: ${value}`);
  });

  await dp.Debugger.enable();
  session.evaluate('debugger;');
  let paused_event = await dp.Debugger.oncePaused();
  const callFrameId = paused_event.params.callFrames[0].callFrameId;

  const consoleHelper = new ConsoleTestHelper(testRunner, dp,
    (expression) => dp.Debugger.evaluateOnCallFrame({ expression, callFrameId })
  );

  await testRunner.runTestSuite([
    async function testConsoleAssertWhilePaused() {
      await consoleHelper.testConsoleAssert();
    },
    async function testConsoleTimeWhilePaused() {
      await consoleHelper.testConsoleTime();
    },
    async function testConsoleCountWhilePaused() {
      await consoleHelper.testConsoleCount();
    },
  ]);

  await dp.Debugger.resume();
  testRunner.completeTest();
});