(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(`Tests that requesting an unknown Digital Credentials protocol causes a deprecation issue`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate("navigator.credentials.get({digital: {requests: [{protocol: 'unknown-protocol', data: {}}]}}).catch(() => {})");
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})
