// Copyright 2024 The Cobalt Authors.All Rights Reserved.
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
//
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('mediasource_worker_util.js');

onmessage = function(event) {
  postMessage({subject: 'error', info: 'No message expected by Worker'});
};

const util = new MediaSourceWorkerUtil();
const handle = URL.createObjectURL(util.mediaSource);

util.mediaSource.addEventListener('sourceopen', () => {
  URL.revokeObjectURL(handle);

  const sourceBuffer =
      util.mediaSource.addSourceBuffer(util.mediaMetadata.type);
  sourceBuffer.addEventListener('error', (err) => {
    postMessage({subject: 'error', info: err});
  });
  function updateEndOnce() {
    sourceBuffer.removeEventListener('updateend', updateEndOnce);
    sourceBuffer.addEventListener('updateend', () => {
      util.mediaSource.duration = 0.5;
      util.mediaSource.endOfStream();
      // Sanity check the duration.
      // Unnecessary for this buffering, except helps with test coverage.
      const duration = util.mediaSource.duration;
      if (isNaN(duration) || duration <= 0.0 || duration >= 1.0) {
        postMessage({
          subject: 'error',
          info: 'mediaSource.duration ' + duration +
              ' is not within expected range (0,1)'
        });
      }
    });

    // Reset the parser. Unnecessary for this buffering, except helps with test
    // coverage.
    sourceBuffer.abort();
    // Shorten the buffered media and test playback duration to avoid timeouts.
    sourceBuffer.remove(0.5, Infinity);
  }
  sourceBuffer.addEventListener('updateend', updateEndOnce);
  util.mediaLoadPromise.then(mediaData => {
    sourceBuffer.appendBuffer(mediaData);
  }, err => {postMessage({subject: 'error', info: err})});
});

postMessage({subject: 'handle', info: handle});
