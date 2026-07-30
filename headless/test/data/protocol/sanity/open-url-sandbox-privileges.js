// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  // Auto-attach to newly created browser targets to catch the spawned window.
  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  bp.Target.onAttachedToTarget(async (event) => {
    const targetInfo = event.params.targetInfo;
    if (targetInfo.type === 'page' && targetInfo.openerId) {
      const pProtocol =
          new TestRunner.Session(testRunner, event.params.sessionId).protocol;
      await pProtocol.Runtime.enable();
      // Verify that the spawned window inherited the initiator's sandbox flags.
      // Because the iframe below has no 'allow-same-origin' sandbox flag, the
      // resulting child window must also run sandboxed with an opaque origin
      // ('null').
      const result =
          await pProtocol.Runtime.evaluate({expression: 'window.origin'});
      const origin = result.result.result.value;
      if (origin === 'null') {
        testRunner.log('PASS\n');
      } else {
        testRunner.log(`FAIL: unexpected origin ${origin}\n`);
      }
      testRunner.completeTest();
    }
  });

  // A page running inside a sandboxed iframe without 'allow-same-origin'
  // dispatches an auxiliary click (middle-click) to spawn a new foreground tab
  // via OpenURLFromTab().
  const html = `
      <iframe sandbox="allow-scripts allow-popups" srcdoc="
          <a href='${
      testRunner._testBaseURL}resources/title.html' target='_blank'>Click</a>
          <script>
            const a = document.querySelector('a');
            const event = new MouseEvent('click', {button: 1, which: 2});
            a.dispatchEvent(event);
          </script>
      "></iframe>
  `;

  testRunner.startHTML(
      html, 'Tests OpenURLFromTab target inherits sandbox privileges.');
})
