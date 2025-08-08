(async function(testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
      `Verifies that CSP issue is created from a page with eval() usage.\n`);

    await dp.Network.enable();
    await dp.Audits.enable();
    page.navigate('https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-eval.php');
    const issue = await dp.Audits.onceIssueAdded();

    testRunner.log(issue.params, "Inspector issue: ");
    testRunner.completeTest();
  })