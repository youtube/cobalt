// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

const interceptedBody = 'window.activeServiceWorker.postMessage("script-intercepted");';
let shouldIntercept = false;

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

self.addEventListener("install", event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  self.clients.claim();
});

const noop = () => {};
const fail = msg => {
  if (msg) {
    console.error(msg);
  }
  postMessage('FAIL');
};
const assertEquals = (expected, actual, msg) => {
  if (expected === actual) {
    return;
  }
  const errorMessage = `Expected: '${expected}', but was '${actual}'`;
  fail(msg ? `${msg}(${errorMessage})` : errorMessage);
};
const assertNotEquals = (notExpected, actual, msg) => {
  if (notExpected !== actual) {
    return;
  }
  const errorMessage = `Not expected: '${notExpected}'`;
  fail(msg ? `${msg}(${errorMessage})` : errorMessage);
};

const ASSET_CACHE_NAME = 'asset-cache-name';
const RESOURCE = self.location.pathname;
const RESOURCE_WITH_QUERY = self.location.pathname + '?foo=bar';
const RESOURCE_NOT_INDIRECTLY_CACHED = self.location.pathname + '?should_not_resolve_to_resource1';
const RESOURCE_NOT_PRESENT = self.location.pathname + '.not_present';

let assetCache = null;
const getAssetCache = async () => {
  if (!assetCache) {
    assetCache = await self.caches.open(ASSET_CACHE_NAME);
  }
  return assetCache;
};
const cacheUrl = async (cache, url) => {
  if (await cache.match(url)) {
    return;
  }
  return cache.add(url);
};
const cacheAssetUrls = async urls => {
  const cache = await getAssetCache();
  return Promise.all(urls.map(url => cacheUrl(cache, url).catch(noop)));
};

self.addEventListener('message', async event => {
  if (event.data === 'start-test') {
    await self.caches.delete(ASSET_CACHE_NAME);
    await cacheAssetUrls([RESOURCE, RESOURCE_WITH_QUERY, RESOURCE_NOT_PRESENT]);
    const cache = await self.caches.open(ASSET_CACHE_NAME);
    assertEquals(2, (await cache.keys()).length);
    assertNotEquals(undefined, await cache.match(RESOURCE));
    assertNotEquals(undefined, await cache.match(RESOURCE_WITH_QUERY));
    assertEquals(undefined, await cache.match(RESOURCE_NOT_INDIRECTLY_CACHED));
    assertEquals(undefined, await cache.match(RESOURCE_NOT_PRESENT));
    postMessage('SUCCESS');
    return;
  }
  postMessage(`${JSON.stringify(event.data)}-unexpected-message`);
});
