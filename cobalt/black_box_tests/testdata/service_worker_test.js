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
self.oninstall = function (e) {
  console.log('oninstall event received', e);
}
self.onactivate = function (e) {
  console.log('onactivate event received', e);
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

console.log('Worker importing scripts invalid URL.');

try {
  self.importScripts('...:...');
} catch (e) {
  message = 'Expected exception message: ' + e;
  console.log(message);
}

console.log('self.clients: ', self.clients);

self.clients.get('foo').then(function (clients) {
  console.log('(Unexpected) self.clients.get(): ', clients);
}, function (error) {
  console.log(`(Expected) self.clients.get() not yet supported: ${error}`, error);
});

self.clients.matchAll().then(function (clients) {
  console.log('(Unexpected) self.clients.matchAll(): ', clients);
}, function (error) {
  console.log(`(Expected) self.clients.matchAll() not yet supported: ${error}`, error);
});


var options = {
  includeUncontrolled: true, type: 'worker'
};

self.clients.matchAll(options).then(function (clients) {
  console.log('(Unexpected) self.clients.matchAll(): ', clients);
}, function (error) {
  console.log(`(Expected) self.clients.matchAll() not yet supported: ${error}`, error);
});

self.clients.claim().then(function (clients) {
  console.log('(Unexpected) self.clients.claim(): ', clients);
}, function (error) {
  console.log(`(Expected) self.clients.claim() not yet supported: ${error}`, error);
});

console.log('Worker importing scripts sunnyday.');

self.importScripts('service_worker_test_importscripts_1.js',
  'service_worker_test_importscripts_2.js',
  'service_worker_test_importscripts_3.js');
