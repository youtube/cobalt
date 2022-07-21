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
    includeUncontrolled: false, type: 'window'
  };
  self.clients.matchAll(options).then(function (clients) {
    for (var i = 0; i < clients.length; i++) {
      message = `Service Worker received a message : ${event.data}`;
      console.log('Posting to client:', message);
      clients[i].postMessage(message);
    }
  });
}

self.oninstall = function (e) {
  console.log('oninstall event received', e);

  console.log('self.clients.claim()');
  e.waitUntil(self.clients.claim().then(function (clients) {
    console.log('(Unexpected) self.clients.claim():', clients);
  }, function (error) {
    console.log(`(Expected) self.clients.claim() not yet activated: ${error}`, error);
  }));

}
self.onactivate = function (e) {
  console.log('onactivate event received', e);

  // Claim should pass here, since the state is activating.
  console.log('self.clients.claim()');
  e.waitUntil(self.clients.claim().then(function (clients) {
    console.log('(Expected) self.clients.claim():', clients);

    var options = {
      includeUncontrolled: false, type: 'window'
    };
    console.log('self.clients.matchAll(options)');
    e.waitUntil(self.clients.matchAll(options).then(function (clients) {
      console.log('(Expected) self.clients.matchAll():', clients.length, clients);
      for (var i = 0; i < clients.length; i++) {
        console.log('Client with url', clients[i].url,
          'frameType', clients[i].frameType,
          'id', clients[i].id,
          'type', clients[i].type);
      }
    }, function (error) {
      console.log(`(Unexpected) self.clients.matchAll(): ${error}`, error);
    }));

  }, function (error) {
    console.log(`(Unexpected) self.clients.claim(): ${error}`, error);
  }));

}
console.log('self.registration', self.registration);
console.log('self.serviceWorker', self.serviceWorker);

console.log('Worker importing scripts with syntax error.');

try {
  self.importScripts(
    'service_worker_test_importscripts_1.js',
    'service_worker_test_importscripts_with_syntax_error.js',
    'service_worker_test_importscripts_3.js');
} catch (e) {
  message = 'Expected exception message: ' + e;
  console.log(message);
}

console.log('Worker importing scripts with invalid URL.');

try {
  self.importScripts('...:...');
} catch (e) {
  message = 'Expected exception message: ' + e;
  console.log(message);
}

console.log('self.clients:', self.clients);

console.log('self.clients.get()');
self.clients.get('foo').then(function (client) {
  // Here, client is expected to be undefined or null.
  console.log('(Expected) self.clients.get():', client);
}, function (error) {
  console.log(`(Unexpected) self.clients.get(): ${error}`, error);
});

console.log('self.clients.matchAll()');
self.clients.matchAll().then(function (clients) {
  console.log('(Expected) self.clients.matchAll():', clients.length, clients);
  // Note: This will return 0 clients if none are controlled so far.
  for (var i = 0; i < clients.length; i++) {
    console.log('Client with url', clients[i].url,
      'frameType', clients[i].frameType,
      'id', clients[i].id,
      'type', clients[i].type);
  }
}, function (error) {
  console.log(`(Unexpected) self.clients.matchAll(): ${error}`, error);
});

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

    console.log('self.clients.get()');
    self.clients.get(clients[i].id).then(function (client) {
      console.log('(Expected) self.clients.get():', client);
      console.log('Client with url', client.url,
        'frameType', client.frameType,
        'id', client.id,
        'type', client.type);
    }, function (error) {
      console.log(`(Unexpected) self.clients.get(): ${error}`, error);
    });
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

console.log('Worker importing scripts sunnyday.');

self.importScripts('service_worker_test_importscripts_1.js',
  'service_worker_test_importscripts_2.js',
  'service_worker_test_importscripts_3.js');
