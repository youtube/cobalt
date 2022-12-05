// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const postMessage = message => {
  const options = {
    includeUncontrolled: false, type: 'window'
  };
  self.clients.matchAll(options).then(clients => {
    clients.forEach(c => {
      c.postMessage(message);
    });
  });
};

const postError = message => {
  postMessage(new Error(message).stack);
};

const assertEquals = (expected, actual, message) => {
  if (expected !== actual) {
    postError(`\nExpected: ${expected}\nActual:   ${actual}\n${message || ''}`);
    throw new Error();
  }
};

const assertTrue = (actual, message) => {
  assertEquals(true, actual, message);
};

self.addEventListener("install", event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  self.clients.claim();
});

self.addEventListener('message', async event => {
  if (event.data === 'start-test') {
    const cache = await caches.open('test-cache');
    if ((await cache.keys()).length !== 0) {
      const keys = await cache.keys();
      await Promise.all(keys.map(k => cache.delete(k.url)));
    }
    assertEquals(0, (await cache.keys()).length);
    await cache.put('http://www.example.com/a', new Response('a'));
    await cache.put('http://www.example.com/b', new Response('b'));
    await cache.put('http://www.example.com/c', new Response('c'));
    const keys = await cache.keys();
    assertEquals(3, keys.length);
    const urls = new Set(keys.map(k => k.url));
    assertTrue(urls.has('http://www.example.com/a'));
    assertTrue(urls.has('http://www.example.com/b'));
    assertTrue(urls.has('http://www.example.com/c'));
    postMessage('end-test');
    return;
  }
  postMessage(`${JSON.stringify(event.data)}-unexpected-message`);
});
