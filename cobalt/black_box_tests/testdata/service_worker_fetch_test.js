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

let fetchEventCount = 0;
let exceptScriptIntercepted = false;

self.addEventListener('message', event => {
  if (event.data === 'start-test') {
    shouldIntercept = true;
    postMessage({test: 'check-fetch-intercepted'});
    return;
  }
  if (event.data.test === 'check-fetch-intercepted') {
    if (interceptedBody !== event.data.result) {
      postMessage({test: 'check-fetch-intercepted', result: 'failed'});
      return;
    }
    shouldIntercept = false;
    postMessage({test: 'check-fetch-not-intercepted'});
    return;
  }
  if (event.data.test === 'check-fetch-not-intercepted') {
    if (interceptedBody === event.data.result) {
      postMessage({test: 'check-fetch-not-intercepted', result: 'failed'});
      return;
    }
    shouldIntercept = true;
    exceptScriptIntercepted = true;
    postMessage({test: 'check-script-intercepted'});
    return;
  }
  if (event.data === 'script-intercepted') {
    if (!exceptScriptIntercepted) {
      postMessage('script-intercepted-unexpected');
    }
    shouldIntercept = false;
    exceptScriptIntercepted = false;
    postMessage({test: 'check-script-not-intercepted'})
    return;
  }
  if (event.data === 'script-not-intercepted') {
    if (exceptScriptIntercepted) {
      postMessage('script-not-intercepted-unexpected');
      return;
    }
    if (fetchEventCount !== 4) {
      postMessage('fetch-event-count-incorrect');
      return;
    }
    postMessage('end-test');
    return;
  }
  postMessage(`${JSON.stringify(event.data)}-unexpected-message`);
});

self.addEventListener('fetch', event => {
  fetchEventCount++;
  if (shouldIntercept) {
    event.respondWith(new Promise(resolve => {
      setTimeout(() => resolve(new Response(interceptedBody)), 100);
    }));
  }
});
