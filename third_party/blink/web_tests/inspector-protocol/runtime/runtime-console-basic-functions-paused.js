(async function testRemoteObjects(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests ConsoleOM APIs when debugger is paused.');
  dp.Runtime.enable();
  const console_argsRequired = [
    'log',
    'debug',
    'info',
    'error',
    'warn',
    'dir',
    'dirxml',
    'table'
  ];
  const console_argsOptional = [
    'trace',
    'clear',
    'group',
    'groupCollapsed',
    'groupEnd'
  ];

  dp.Runtime.onConsoleAPICalled(result => testRunner.log(result));

  await dp.Debugger.enable();
  session.evaluate('debugger;');
  let paused_event = await dp.Debugger.oncePaused();
  const callFrameId = paused_event.params.callFrames[0].callFrameId;
  for (let func of console_argsRequired)
    await testConsoleFunctionsArgs(func, true, callFrameId);
  for (let func of console_argsOptional)
    await testConsoleFunctionsArgs(func, false, callFrameId);
  await dp.Debugger.resume();
  testRunner.completeTest();

  async function testConsoleFunctionsArgs(func, required, callFrameId) {
    logConsoleTestMethod(func, required);
    await dp.Debugger.evaluateOnCallFrame({ expression: `console.${func}(10, Infinity, {a:3, b:"hello"})`, callFrameId });
    await dp.Debugger.evaluateOnCallFrame({ expression: `console.${func}()`, callFrameId });
  }

  function logConsoleTestMethod(func, required) {
    const argType = required ? "required" : "optional";
    testRunner.log(`Testing console.${func} with ${argType} args while paused`);
  }

});