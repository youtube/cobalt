// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `
      <html>
      <body lang="en"></body>
      <script>
        function openPopupAndCheckOpener() {
          const win = window.open('about:blank');
          return win && win.opener === window;
        }
      </script>
    </html>
  `;
  const {session} = await testRunner.startHTML(
      html, 'Tests pop up window has window.opener.');

  const result = await session.evaluate('window.openPopupAndCheckOpener();');

  if (result) {
    testRunner.log('PASS\n');
  } else {
    testRunner.log('FAIL\n');
  }

  testRunner.completeTest();
})
