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

"use strict";

const QUOTA_EXCEEDED_ERROR_CODE = 22;
const mimeCodec = 'video/webm; codecs="vp9"';
const assetURL = 'vp9-720p.webm';

const assetDuration = 15;
const assetSize = 344064;

let status_div;
let video;


function fetchAB(url, cb) {
  console.log("Fetching.. ", url);
  const xhr = new XMLHttpRequest();
  xhr.open("get", url);
  xhr.responseType = "arraybuffer";
  xhr.onload = () => {
    console.log("onLoad - calling Callback");
    cb(xhr.response);
  };
  console.log('Sending request for media segment ...');
  xhr.send();
}

function testAppendToBuffer(media_source, mem_limit) {
  const mediaSource = media_source;
  const sourceBuffer = mediaSource.addSourceBuffer(mimeCodec);
  if (mem_limit > 0) {
    status_div.innerHTML += "Test SourceBuffer, setting memoryLimit to " + mem_limit + "<br>";
    sourceBuffer.memoryLimit = mem_limit;
  } else {
    status_div.innerHTML += "Test SourceBuffer, leaving memoryLimit at default<br>";
  }

  let MIN_SIZE = 12 * 1024 * 1024;
  let ESTIMATED_MIN_TIME = 12;

  fetchAB(assetURL, (buf) => {
    let expectedTime = 0;
    let expectedSize = 0;
    let appendCount = 0;


    let onBufferFull = function(buffer_was_full) {
      console.log("OnBufferFull! Quota exceeded? " + buffer_was_full + " appendCount:" + appendCount + " expectedTime:" + expectedTime);
      status_div.innerHTML += "Finished! Quota exceeded? " + buffer_was_full + " appendCount:" + appendCount + " appended " + appendCount * assetSize + "<br>";
    }

    sourceBuffer.addEventListener("updateend", function onupdateend() {
      console.log("Update end. State is " + sourceBuffer.updating);
      appendCount++;
      console.log("Append Count" + appendCount);
      if (sourceBuffer.buffered.start(0) > 0 || expectedTime > sourceBuffer.buffered.end(0)) {
         sourceBuffer.removeEventListener('updatedend', onupdateend);
         onBufferFull(false);
      } else {
        expectedTime +=  assetDuration;
        expectedSize += assetSize;
        if (expectedSize > (10 * MIN_SIZE)) {
          sourceBuffer.removeEventListener('updateend', onupdateend);
          onBufferFull(false);
          return;
        }

        try {
          sourceBuffer.timestampOffset = expectedTime;
        } catch(e) {
          console.log("Unexpected error: " + e);
        }

        try {
          sourceBuffer.appendBuffer(buf);
        } catch(e) {
          console.log("Wuff! QUOTA_EXCEEDED_ERROR!");
          status_div.innerHTML += "Wuff! QUOTA_EXCEEDED<br>";
          if (e.code == QUOTA_EXCEEDED_ERROR_CODE) {
            sourceBuffer.removeEventListener('updateend', onupdateend);
            onBufferFull(true);
          } else {
            console.log("Unexpected error: " + e);
          }
        }
      }
    });

    console.log("First Append!");
    sourceBuffer.appendBuffer(buf);
    status_div.innerHTML += "First append. MemoryLimit is:" + sourceBuffer.memoryLimit + ".<br>";
  });
}

function onSourceOpen() {
  console.log("Source Open. This state:", this.readyState); // open
  status_div.innerHTML += "Source Open. This state:" + this.readyState + "<br>";
  status_div.innerHTML += "Lets test first source_buffer, defaults..<br>";
  testAppendToBuffer(this, 0);

  let new_mem_limit = 400 * 1024 * 1024;
  status_div.innerHTML += "<br><br>Lets test second source_buffer, setting memory to:" + new_mem_limit + "<br>";
  testAppendToBuffer(this, new_mem_limit);
  video.play();
}


function createMediaSource() {

  console.log('Video Get Element By Id...');
  video = document.getElementById('video');
  status_div = document.getElementById('status');
  status_div.innerHTML += 'Video Get Element By Id...<br>';

  console.log('Create Media Source...');
  status_div.innerHTML += 'Create Media Source...<br>';
  var mediaSource = new MediaSource;

  console.log('Attaching MediaSource to video element ...');
  status_div.innerHTML += 'Attaching MediaSource to video element ...<br>';
  video.src = window.URL.createObjectURL(mediaSource);

  console.log('Add event listener..');
  status_div.innerHTML += 'Add event listener..<br>';
  mediaSource.addEventListener('sourceopen', onSourceOpen);

}

addEventListener('load', createMediaSource);
