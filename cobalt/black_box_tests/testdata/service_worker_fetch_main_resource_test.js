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

const postMessage = message => {
  const options = {
    includeUncontrolled: false,
    type: 'window',
  };
  self.clients.matchAll(options).then(clients => {
    clients.forEach(c => {
      c.postMessage(message);
    });
  });
};

self.addEventListener("install", () => self.skipWaiting());
self.addEventListener('activate', () => self.clients.claim());

const fetchRequests = [];
self.addEventListener('message', () => postMessage(fetchRequests));
self.addEventListener('fetch', ({request: {url, method, mode, headers}}) => {
  fetchRequests.push({
    url, method, mode, headers: Array.from(headers.entries()),
  });
});
