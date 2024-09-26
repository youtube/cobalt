// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptPolicy: TrustedTypePolicy =
    window.trustedTypes!.createPolicy('webui-test-script', {
      createHTML: () => '',
      createScriptURL: urlString => {
        const url = new URL(urlString);
        if (url.protocol === 'chrome:') {
          return urlString;
        }

        console.error(`Invalid test URL ${urlString} found.`);
        return '';
      },
      createScript: () => '',
    });

/**
 * @return Whether a test module was loaded.
 *   - In case where a module was not specified, returns false (used for
 *     providing a way for UIs to wait for any test initialization, if run
 *     within the context of a test).
 *   - In case where loading failed (either incorrect URL or incorrect "host="
 *     parameter) a rejected Promise is returned.
 */
export function loadTestModule(): Promise<boolean> {
  const params = new URLSearchParams(window.location.search);
  const module = params.get('module');
  if (!module) {
    return Promise.resolve(false);
  }

  const host = params.get('host') || 'webui-test';
  if (host !== 'test' && host !== 'webui-test') {
    return Promise.reject(new Error(`Invalid host=${host} parameter`));
  }

  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.type = 'module';
    const src = `chrome://${host}/${module}`;
    script.src = scriptPolicy.createScriptURL(src) as unknown as string;
    script.onerror = function() {
      reject(new Error(`test_loader_util: Failed to load ${src}`));
    };
    script.onload = function() {
      resolve(true);
    };
    document.body.appendChild(script);
  });
}
