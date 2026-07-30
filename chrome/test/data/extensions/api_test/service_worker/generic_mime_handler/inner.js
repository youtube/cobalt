// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs in a cross-origin HTTPS iframe nested inside the MIME-handler
// OOPIF (chrome-extension://<id>/handler.html). The browsertest uses
// this script to verify that the embedded HTTPS subframe is treated as
// a secure context even when the top-level navigation went to a non-
// trustworthy host (the MIME-handler secure-context fix gated by
// `blink::features::kMimeHandlerSecureContextDerivationFromBrowser`).
//
// The probe reports back to the parent (handler.html) via postMessage:
//   - `isSecureContext`     -- window.isSecureContext as seen here.
//   - `hasServiceWorkerAPI` -- navigator.serviceWorker is exposed (only
//                              exposed in secure contexts).
//   - `registered`          -- SW registration resolved; only set when
//                              the SW API is available.
//   - `fetchOk`             -- a probe fetch to "inner-probe" reached
//                              the SW and was intercepted (HTTP 200).
//   - `error`               -- stringified error if the SW dance threw
//                              or timed out.

const kSwDanceTimeoutMs = 5000;

function withTimeout(p, label) {
  return Promise.race([
    p,
    new Promise(
        (_, rej) => setTimeout(
            () => rej(new Error(label + ' timeout')), kSwDanceTimeoutMs)),
  ]);
}

const report = {
  kind: 'inner-report',
  isSecureContext: window.isSecureContext,
  hasServiceWorkerAPI: !!navigator.serviceWorker,
};

async function run() {
  if (!report.hasServiceWorkerAPI) {
    return;
  }
  try {
    await withTimeout(
        navigator.serviceWorker.register('inner-sw.js'), 'register');
    await withTimeout(navigator.serviceWorker.ready, 'ready');
    report.registered = true;
    // `serviceWorker.ready` resolves when there is an active worker, but
    // does not guarantee that this client is being controlled by it. If
    // we register a brand-new SW that calls `clients.claim()` in
    // `activate`, the claim is asynchronous; without waiting for it the
    // subsequent fetch can race ahead and bypass the SW entirely.
    if (!navigator.serviceWorker.controller) {
      await withTimeout(
          new Promise(resolve => {
            navigator.serviceWorker.addEventListener(
                'controllerchange', resolve, {once: true});
          }),
          'claim');
    }
    const resp = await withTimeout(fetch('inner-probe'), 'fetch');
    report.fetchOk = resp.ok;
  } catch (e) {
    report.error = String(e);
  }
}

run().finally(() => window.parent.postMessage(report, '*'));
