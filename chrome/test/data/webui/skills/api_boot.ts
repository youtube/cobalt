// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SKILLS_HANDSHAKE_ACK, SKILLS_HANDSHAKE_TYPE} from 'chrome://skills/v2/skills_webview_bridge_constants.js';

/**
 * Listens for a handshake message from Chrome and replies with an
 * acknowledgement. This should be called on or before page load. The returned
 * promise resolves once the connection is established.
 */
export function createSkillsHostProxyOnLoad(
    allowedHostOrigin: string = 'chrome://skills'): Promise<void> {
  return new Promise<void>((resolve) => {
    function messageHandler(event: MessageEvent) {
      if (!event.source) {
        return;
      }

      if (event.origin !== allowedHostOrigin) {
        return;
      }

      const msg = event.data;
      if (!msg) {
        return;
      }

      if (msg.type === SKILLS_HANDSHAKE_TYPE) {
        window.removeEventListener('message', messageHandler);

        const chromeHostWindow = event.source as WindowProxy;
        const chromeHostOrigin = event.origin;

        // Send ACK back.
        chromeHostWindow.postMessage(
            {
              type: SKILLS_HANDSHAKE_ACK,
            },
            chromeHostOrigin);

        resolve();
      }
    }

    window.addEventListener('message', messageHandler);
  });
}
