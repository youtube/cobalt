// Copyright 2022 The Cobalt Authors.All Rights Reserved.
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

const data = 'web worker test loaded'
self.postMessage(data);
self.onmessage = function (event) {
    let message = `worker received ${event.data}`;
    console.log(message);
    if (event.data == 'import scripts now') {
        // These should load and execute synchronously.
        console.log('Worker importing scripts.');
        self.importScripts('web_worker_test_importscripts_1.js',
            'web_worker_test_importscripts_2.js',
            'web_worker_test_importscripts_3.js');
    }
    // These messages should not race messages synchronously posted from the
    // importScripts above.
    self.postMessage(message);
    const uppercase_data = event.data.toUpperCase();
    self.postMessage(uppercase_data);
};

try {
    self.importScripts(
        'web_worker_test_importscripts_1.js',
        'web_worker_test_importscripts_with_syntax_error.js',
        'web_worker_test_importscripts_3.js');
} catch (e) {
    message = 'Expected exception message 4: ' + e;
    console.log(message);
    self.postMessage(message);
}

try {
    self.importScripts('...:...');
} catch (e) {
    message = 'Expected exception message 5: ' + e;
    console.log(message);
    self.postMessage(message);
}

this
