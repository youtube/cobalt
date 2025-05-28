// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that an attributionsrc response with an invalid Attribution-Reporting-Register-Source header triggers an issue.');

  await dp.Audits.enable();

  session.evaluate(`
    document.body.innerHTML = '<img attributionsrc="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-invalid-source.php">'
  `);

  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
