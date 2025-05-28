// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(
      '<a href="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-source-with-scopes-redirect.php" attributionsrc target="_blank">Link</a>',
      'Test that registering two sources with different scopes in the same navigation triggers an issue.');

  await dp.Audits.enable();

  session.evaluateAsyncWithUserGesture(`document.querySelector('a').click()`);

  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);

  testRunner.completeTest();
});
