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

console.log('Service Worker Script Started');
self.onmessage = function (event) {
  console.log('Got onmessage event', event, JSON.stringify(event.data), event.source, event.target);
  if (event instanceof ExtendableMessageEvent) {
    var options = {
      includeUncontrolled: false, type: 'window'
    };
    event.waitUntil(self.clients.matchAll(options).then(function (clients) {
      for (var i = 0; i < clients.length; i++) {
        message = { text: 'Foo', bar: 'BarString' };
        console.log('Posting to client:', JSON.stringify(message));
        clients[i].postMessage(message);
      }
    }));
    event.waitUntil(self.clients.matchAll(options).then(function (clients) {
      message = {
        text: 'Service Worker received a message from source',
        data: event.data,
        visibilityState: event.source.visibilityState,
        focused: event.source.focused,
        event_type: `${event}`
      }
      console.log('Posting to client:', JSON.stringify(message));
      event.source.postMessage(message);
      for (var i = 0; i < clients.length; i++) {
        message = {
          text: 'Service Worker received a message from client',
          data: event.data,
          visibilityState: clients[i].visibilityState,
          focused: clients[i].focused,
          event_type: `${event}`
        }
        console.log('Posting to client:', JSON.stringify(message));
        clients[i].postMessage(message);
      }
    }));
  }
}

self.onactivate = function (e) {
  e.waitUntil(self.clients.claim().then(function (result) {
    console.log('(Expected) self.clients.claim():', result);
  }, function (error) {
    console.log(`(Unexpected) self.clients.claim(): ${error}`, error);
  }));
}
