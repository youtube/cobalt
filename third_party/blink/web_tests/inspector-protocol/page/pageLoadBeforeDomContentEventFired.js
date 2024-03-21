(async function (testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    'Tests Page.loadEventFired is triggered before Page.domContentEventFired for a blank page');

  await dp.Page.enable();

  const htmlPage = testRunner.url('../resources/blank.html');
  dp.Page.navigate({ url: htmlPage });
  const loadEvent = await dp.Page.onceLoadEventFired();
  testRunner.log(loadEvent);
  const domContentEvent = await dp.Page.onceDomContentEventFired();
  testRunner.log(domContentEvent);

  testRunner.completeTest();
})
