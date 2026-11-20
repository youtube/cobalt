// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service worker for the HTTPS inner iframe. Intercepts a single probe
// URL ("inner-probe") to prove that navigator.serviceWorker is usable
// from the iframe and that the SW is its controller.

self.addEventListener('install', e => e.waitUntil(self.skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));
self.addEventListener('fetch', e => {
  if (new URL(e.request.url).pathname.endsWith('/inner-probe')) {
    e.respondWith(new Response('intercepted', {status: 200}));
  }
});
