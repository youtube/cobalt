(async function(testRunner) {
  const test = await testRunner.loadScript(
      'resources/service-workers/service-worker-test.js');
  await test(
      testRunner,
      {description: 'disllowed redirect', type: 'classic', redirectTo: '/foo'});
})
