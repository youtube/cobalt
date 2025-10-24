'use strict';

self.addEventListener('install', async e => {
  await e.addRoutes(
      [{condition: {urlPattern: new URLPattern({})}, source: 'fetch-event'}]);
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  e.respondWith(new Response('fetch event'));
});
