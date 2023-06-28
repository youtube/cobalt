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

const assertEqual = (expected, actual) => {
  if (expected !== actual) {
    postMessage(`${actual} does not equal expected ${expected}`);
  }
};

const sharedArrayBufferFromWorker = new SharedArrayBuffer(5);
const sharedArrayBufferFromWorkerView = new Uint8Array(sharedArrayBufferFromWorker);
let sharedArrayBufferFromWindow = null;
let sharedArrayBufferFromWindowView = null;

self.addEventListener('message', event => {
  if (event.data.type === 'check-data') {
    const data = event.data.data;
    assertEqual(1, data.number);
    assertEqual(2, data.array.length);
    assertEqual(2, data.array[0]);
    assertEqual(3, data.array[1]);
    assertEqual('abc', data.string);
    assertEqual(3, data.arrayBuffer.byteLength);
    const arrayBufferFromWindowView = new Uint8Array(data.arrayBuffer);
    assertEqual(4, arrayBufferFromWindowView[0]);
    assertEqual(5, arrayBufferFromWindowView[1]);
    assertEqual(6, arrayBufferFromWindowView[2]);
    assertEqual(4, data.sharedArrayBuffer.byteLength);
    sharedArrayBufferFromWindow = data.sharedArrayBuffer;
    sharedArrayBufferFromWindowView = new Uint8Array(sharedArrayBufferFromWindow);
    const arrayBuffer = new ArrayBuffer(3);
    const arrayBufferView = new Uint8Array(arrayBuffer);
    arrayBufferView[0] = 4;
    arrayBufferView[1] = 5;
    arrayBufferView[2] = 6;
    postMessage({
      type: 'check-data',
      data: {
        number: 1,
        array: [2, 3],
        string: 'abc',
        arrayBuffer,
        sharedArrayBuffer: sharedArrayBufferFromWorker,
      },
    });
    return;
  }
  if (event.data.type === 'update-shared-array-buffers') {
    const {index, value} = event.data;
    sharedArrayBufferFromWindowView[index] = value;
    sharedArrayBufferFromWorkerView[index] = value;
    postMessage({type: 'update-shared-array-buffers-done'});
    return;
  }
  if (event.data.type === 'check-shared-array-buffers') {
    const {index, value} = event.data;
    assertEqual(value, sharedArrayBufferFromWindowView[index]);
    assertEqual(value, sharedArrayBufferFromWorkerView[index]);
    postMessage({type: 'check-shared-array-buffers-done'});
    return;
  }
  postMessage(`Unexpected message ${JSON.stringify(event.data)}.`);
});
