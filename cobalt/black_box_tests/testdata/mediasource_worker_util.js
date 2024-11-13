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

// This script is intended to be imported into a worker's script, and provides
// common preparation for multiple test cases. Errors encountered are either
// postMessaged with subject of 'error', or in the case of failed
// mediaLoadPromise, result in promise rejection.

if (!this.MediaSource)
  postMessage({subject: 'error', info: 'MediaSource API missing from Worker'});

const MEDIA_LIST = [
  {
    url: 'video/test.mp4',
    type: 'video/mp4; codecs="mp4a.40.2,avc1.4d400d"',
  },
  {
    url: 'video/test.webm',
    type: 'video/webm; codecs="vp8, vorbis"',
  },
];

class MediaSourceWorkerUtil {
  constructor() {
    this.mediaSource = new MediaSource();

    // Find supported test media, if any.
    this.foundSupportedMedia = false;
    for (let i = 0; i < MEDIA_LIST.length; ++i) {
      this.mediaMetadata = MEDIA_LIST[i];
      if (MediaSource.isTypeSupported(this.mediaMetadata.type)) {
        this.foundSupportedMedia = true;
        break;
      }
    }

    // Begin asynchronous fetch of the test media.
    if (this.foundSupportedMedia) {
      this.mediaLoadPromise =
          MediaSourceWorkerUtil.loadBinaryAsync(this.mediaMetadata.url);
    } else {
      postMessage({subject: 'error', info: 'No supported test media'});
    }
  }

  static loadBinaryAsync(url) {
    return new Promise((resolve, reject) => {
      const request = new XMLHttpRequest();
      request.open('GET', url, true);
      request.responseType = 'arraybuffer';
      request.onerror = event => {
        reject(event);
      };
      request.onload = () => {
        if (request.status != 200) {
          reject('Unexpected loadData_ status code : ' + request.status);
        }
        let response = new Uint8Array(request.response);
        resolve(response);
      };
      request.send();
    });
  }
}
