(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests if forciblyPurgeJavascriptMemory destroys execution context.`);

  dp.Runtime.enable();
  dp.Memory.forciblyPurgeJavaScriptMemory();
  await dp.Runtime.exectuionContextDestroyed();
  testRunner.log("Execution context is destroyed.");
  testRunner.completeTest();
})
