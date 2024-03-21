async function wait_for_performance_entries(url) {
  const entries = performance.getEntriesByName(url);
  if (entries.length > 0) {
    return entries;
  }
  return new Promise(resolve => {
    new PerformanceObserver(list => {
      const entries = list.getEntriesByName(url);
      if (entries.length > 0) {
        resolve(entries);
      }
    }).observe({ entryTypes: ['resource'] });
  });
}

self.addEventListener('activate', event => {
  event.waitUntil(self.registration.navigationPreload.enable());
});

self.addEventListener('fetch', event => {
  let headers;
  event.respondWith(
    event.preloadResponse
      .then(response => {
        headers = response.headers;
        return response.text();
      })
      .then(_ => wait_for_performance_entries(event.request.url))
      .then(entries => {
        return new Response(JSON.stringify({timingEntries: entries}),
                            {headers: {'Content-Type': 'text/html'}});
      })
  );
});
