// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(
      '<a href="/inspector-protocol/attribution-reporting/resources/register-web-and-os-source.php" attributionsrc target="_blank">Link</a>',
      'Test that clicking an attributionsrc anchor triggers an issue when it tries to register a web trigger and an OS trigger together.');

  await dp.Audits.enable();

  session.evaluateAsyncWithUserGesture(`document.querySelector('a').click()`);

  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);

  testRunner.completeTest();
});
