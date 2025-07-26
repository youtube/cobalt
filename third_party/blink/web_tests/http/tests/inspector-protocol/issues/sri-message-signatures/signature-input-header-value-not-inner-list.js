// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const valid_signature = `signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==:`;
const valid_signature_input = `signature=("unencoded-digest";sf);keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";tag="sri"`;

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for a `Signature-Input` whose value is not an inner-list.");

  // Navigate to an arbitrary file, enable audits, and wait for an issue to
  // appear. We'll perform the test via the `fetch()` call below.
  await dp.Network.enable();
  await dp.Audits.enable();
  const url = 'inspector-protocol/network/resources/hello-world.html';
  await page.navigate(url);
  const issuePromise = dp.Audits.onceIssueAdded();

  let testURL = new URL('/inspector-protocol/resources/sri-message-signature-test.php', self.origin);
  testURL.searchParams.set('signature', valid_signature);
  testURL.searchParams.set('input', `signature=1`);
  await session.evaluate(`fetch('${testURL.href}')`);

  // Dump the issue:
  const issue = await issuePromise;
  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
