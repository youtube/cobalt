(async function testRemoteObjects(testRunner) {

  const { dp, session } = await testRunner.startBlank('Tests ConsoleOM APIs with unique argument behavior.');

  const ConsoleTestHelper = await testRunner.loadScript('./resources/console-test-helper.js');
  const consoleHelper = new ConsoleTestHelper(testRunner, dp,
    (expression, contextId) => dp.Runtime.evaluate({ expression, contextId })
  );
  dp.Runtime.enable();
  dp.DOM.enable();
  const response1 = await dp.Runtime.onceExecutionContextCreated();
  const pageContextId = response1.params.context.id; // main page
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);

  const response2 = await dp.Runtime.onceExecutionContextCreated();
  const frameContextId = response2.params.context.id; // IFrame

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const firstParamValue = params.args[0].value;
    const value = params.type === "timeEnd" ? firstParamValue.split(':')[0] : firstParamValue;
    testRunner.log(`${params.type}: ${value}`);
  });

  await testRunner.runTestSuite([
    async function testConsoleAssertNoContext() {
      await consoleHelper.testConsoleAssert();
    },
    async function testConsoleTimeNoContext() {
      await consoleHelper.testConsoleTime();
    },
    async function testConsoleCountNoContext() {
      await consoleHelper.testConsoleCount();
    },
    async function testConsoleAssertPageContext() {
      await consoleHelper.testConsoleAssert(pageContextId);
    },
    async function testConsoleTimePageContext() {
      await consoleHelper.testConsoleTime(pageContextId);
    },
    async function testConsoleCountPageContext() {
      await consoleHelper.testConsoleCount(pageContextId);
    },
    async function testConsoleAssertFrameContext() {
      await consoleHelper.testConsoleAssert(frameContextId);
    },
    async function testConsoleTimeFrameContext() {
      await consoleHelper.testConsoleTime(frameContextId);
    },
    async function testConsoleCountFrameContext() {
      await consoleHelper.testConsoleCount(frameContextId);
    },
  ]);
  testRunner.completeTest();


});