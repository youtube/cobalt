self.addEventListener('fetch', fetchEvent => {
  console.log('service worker making fetch for url: ' + fetchEvent.request.url);

  const responsePromise = fetch(fetchEvent.request);

  if (fetchEvent.request.url ===
      'http://127.0.0.1:8000/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.html') {
    fetchEvent.respondWith(responsePromise.then(response => {
      const init = {
        status: response.status,
        statusText: response.statusText,
        headers: {}
      };
      response.headers.forEach((v, k) => {
        init.headers[k] = v;
      });
      return response.text().then(body => {
        const newBody =
            body.replace('</body>', '<p>injected by service worker</p></body>');
        return new Response(newBody, init);
      });
    }));

  } else {
    fetchEvent.respondWith(responsePromise);
  }
});
