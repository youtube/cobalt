// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?Promise<function(!ArrayBuffer): !Promise<!File>>} */
let _piexLoadPromise = null;

/**
 * Loads PIEX, the "Preview Image Extractor", via wasm.
 * @type {!Promise<function(!ArrayBuffer): !Promise<!File>>}
 */
export function loadPiex() {
  async function startLoad() {
    function loadJs(/** string */ path, /** boolean */ module) {
      return new Promise((resolve, reject) => {
        const script =
            /** @type {!HTMLScriptElement} */ (
                document.createElement('script'));
        script.onload = resolve;
        script.onerror = reject;
        if (module) {
          script.type = 'module';
        }
        script.src = path;
        if (document.head) {
          document.head.appendChild(script);
        }  // else not reached (but needed to placate closure).
      });
    }
    await loadJs('piex/piex.js.wasm', false);
    await loadJs('piex_module.js', true);
    return window['extractFromRawImageBuffer'];
  }
  if (!_piexLoadPromise) {
    _piexLoadPromise = startLoad();
  }
  return _piexLoadPromise;
}
