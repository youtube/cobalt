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
  console.log('Got onmessage event', event.source, event.target, event.data);
  var options = {
    includeUncontrolled: true, type: 'window'
  };
  self.clients.matchAll(options).then(function (clients) {
    for (var i = 0; i < clients.length; i++) {
      message = `Service Worker received a message : ${event.data}`;
      console.log('Posting to client:', message);
      clients[i].postMessage(message);
    }
  });
}

function delay_promise(delay) {
  return new Promise(function (resolve) {
    setTimeout(resolve.bind(null), delay)
  });
}

self.oninstall = function (e) {
  console.log('oninstall event received', e);
  // Using a delay long enough to make it clearly visible in the log that the
  // event is extended, and is delaying the activate event and ready promise.
  e.waitUntil(delay_promise(100).then(() => console.log('Promised delay.'), () => console.log('\nPromised rejected.\n')));
}

self.onactivate = function (e) {
  console.log('onactivate event received', e);

  // Claim should pass here, since the state is activating.
  console.log('self.clients.claim()');
  e.waitUntil(self.clients.claim().then(function (result) {
    console.log('(Expected) self.clients.claim():', result);

    var options = {
      includeUncontrolled: false, type: 'window'
    };
  // Using a delay long enough to make it clearly visible in the log that the
  // event is extended.
    e.waitUntil(delay_promise(100).then(function () {
      console.log('self.clients.matchAll(options)');
      e.waitUntil(self.clients.matchAll(options).then(function (clients) {
        console.log('(Expected) self.clients.matchAll():', clients.length, clients);
        for (var i = 0; i < clients.length; i++) {
          console.log('Client with url', clients[i].url,
            'frameType', clients[i].frameType,
            'id', clients[i].id,
            'type', clients[i].type);
          clients[i].postMessage(`You have been claimed, client with id ${clients[i].id}`);
        }
      }, function (error) {
        console.log(`(Unexpected) self.clients.matchAll(): ${error}`, error);
      }));
    }));

  }, function (error) {
    console.log(`(Unexpected) self.clients.claim(): ${error}`, error);
  }));

}
console.log('self.registration', self.registration);
console.log('self.serviceWorker', self.serviceWorker);

console.log('self.clients:', self.clients);

var options = {
  includeUncontrolled: true, type: 'window'
};

console.log('self.clients.matchAll(options)');
self.clients.matchAll(options).then(function (clients) {
  console.log('(Expected) self.clients.matchAll():', clients.length, clients);
  for (var i = 0; i < clients.length; i++) {
    console.log('Client with url', clients[i].url,
      'frameType', clients[i].frameType,
      'id', clients[i].id,
      'type', clients[i].type);
  }
}, function (error) {
  console.log(`(Unexpected) self.clients.matchAll(): ${error}`, error);
});

// Expected to fail since the worker isn't expected to be activated yet.
console.log('self.clients.claim()');
self.clients.claim().then(function (clients) {
  console.log('(Unexpected) self.clients.claim():', clients);
}, function (error) {
  console.log(`(Expected) self.clients.claim() not yet activated: ${error}`, error);
});
