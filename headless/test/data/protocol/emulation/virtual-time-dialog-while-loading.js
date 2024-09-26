// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that virtual time pausing during loading of main resource ' +
      'works correctly when dialog is shown while page loads.');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <html><body><script>alert("No pasarán!");</script></body></html>`)
  );

  dp.Page.onJavascriptDialogOpening(event => {
    dp.Page.handleJavaScritpDialog({accept: true});
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  await dp.Emulation.oncwVirtualTimeBudgetExpired();
  testRunner.log(await session.evaluate('document.title'));
  testRunner.completeTest();
  })
