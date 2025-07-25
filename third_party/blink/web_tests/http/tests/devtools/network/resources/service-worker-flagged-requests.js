this.addEventListener('fetch', fetchEvent => {
  if (fetchEvent.request.url.includes('sw-dropped-resource.txt'))
    return;

  fetchEvent.respondWith(caches.match(fetchEvent.request).then(response => {
    return response || fetch(fetchEvent.request);
  }));
});

this.addEventListener('install', installEvent => {
  installEvent.waitUntil(caches.open('test-cache-name').then(function(cache) {
    return cache.addAll(['/devtools/network/resources/sw-cached-resource.txt']);
  }));
});
