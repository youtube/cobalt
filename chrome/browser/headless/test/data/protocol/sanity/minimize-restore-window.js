// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `<!doctype html>
    <html><script>
    document.addEventListener("visibilitychange", () => {
        console.log(document.visibilityState);
    });
    </script></html>
  `;

  const {session, dp} = await testRunner.startHTML(
      html, `Tests browser window minimize and restore.`);

  await dp.Runtime.enable();

  async function waitForVisibilityChange() {
    for (;;) {
      const result = await dp.Runtime.onceConsoleAPICalled();
      const text = result.params.args[0].value;
      if (text === 'visible' || text === 'hidden')
        break;
    }
  }

  async function logWindowState(text, windowId) {
    const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
    const visibilityState = await session.evaluate(`document.visibilityState`);
    testRunner.log(`${text}: ${bounds.windowState} ${visibilityState}`);
  }

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await logWindowState('Initial', windowId);

  dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'minimized'}});
  await waitForVisibilityChange();
  await logWindowState('Minimized', windowId);

  dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  await waitForVisibilityChange();
  await logWindowState('Restored', windowId);

  testRunner.completeTest();
})
