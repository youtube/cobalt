// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs in the MIME-handler OOPIF (chrome-extension://<id>/handler.html)
// after an application/pdf navigation is routed through the new generic
// MIME handler (`extensions_features::kApiMimeHandler`). Reads the
// originalUrl fragment from `chrome.mimeHandler.getStreamInfo()` and
// injects an HTTPS inner URL into the iframe so the secure-context
// browsertests can probe a cross-process HTTPS descendant of the OOPIF.
// The OOPIF itself runs at chrome-extension://<id>/handler.html (no
// fragment), so the fragment is recovered from `originalUrl` -- the URL
// the user navigated to.
(async () => {
  const info = await chrome.mimeHandler.getStreamInfo();
  const original = new URL(info.originalUrl);
  if (original.hash) {
    document.getElementById('inner').src =
        decodeURIComponent(original.hash.slice(1));
  }
})();

// Forward inner-frame reports out to the browsertest via chrome.test
// (consumed by `ExtensionTestMessageListener`).
window.addEventListener('message', (e) => {
  if (e.data && e.data.kind === 'inner-report') {
    chrome.test.sendMessage('inner-report:' + JSON.stringify(e.data));
  }
});
