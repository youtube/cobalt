// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener((p) => {
  p.onMessage.addListener((m) => {
    if (m == 'pagehide') {
      // Posting a message to cause the BFCache eviction.
      p.postMessage('evict');
    }
  })
});
