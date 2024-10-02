(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the dismissing a FedCm dialog works");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html");

  await dp.FedCm.enable({disableRejectionDelay: true});

  const dialogPromise = session.evaluateAsync("triggerDialog()");
  const msg = await dp.FedCm.onceDialogShown();
  dp.FedCm.dismissDialog({dialogId: msg.params.dialogId});
  // This should be a NetworkError
  testRunner.log(await dialogPromise);
  testRunner.completeTest();
})
