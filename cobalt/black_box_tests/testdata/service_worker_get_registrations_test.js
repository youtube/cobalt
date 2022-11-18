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

self.addEventListener("install", event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  self.clients.claim();
});

self.addEventListener('message', event => {
  if (event.data === 'start-test') {
    postMessage('check-get-registrations');
    return;
  }
  if (event.data === 'check-get-registrations-ok') {
    postMessage('end-test');
    return;
  }
  postMessage(`${JSON.stringify(event.data)}-unexpected-message`);
});
