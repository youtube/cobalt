(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the dialogShown event works after enabling the " +
          "FedCm domain");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html");

  await dp.FedCm.enable({disableRejectionDelay: true});

  const dialogPromise = session.evaluateAsync("triggerDialog()");
  let msg = await dp.FedCm.onceDialogShown();
  if (msg.error) {
    testRunner.log(msg.error);
  } else {
    testRunner.log(msg.params, "msg.params: ", ["dialogId"]);
    dp.FedCm.selectAccount({dialogId: msg.params.dialogId, accountIndex: 0});
    const token = await dialogPromise;
    testRunner.log("token: " + token);
  }
  testRunner.completeTest();
})
