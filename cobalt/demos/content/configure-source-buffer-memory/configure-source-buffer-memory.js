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
// const mimeCodec = 'audio/mp4; codecs="mp4a.40.2"';
// const assetURL = 'fmp4-aac-44100-tiny.mp4';
const mimeCodec = 'video/webm; codecs="vp9"';
const assetURL = 'vp9-720p.webm';
//  var videoBuffer = ms.addSourceBuffer('video/webm; codecs="vp9"; decode-to-texture=true');
//  vp9-720p.webm

const assetDuration = 15;
const assetSize = 344064;

function fetchAB(url, cb) {
  console.log("Fetching.. ", url);
  const xhr = new XMLHttpRequest();
  xhr.open("get", url);
  xhr.responseType = "arraybuffer";
  xhr.onload = () => {
    console.log("ONLOAD - calling CALLBACK");
    cb(xhr.response);
  };
  console.log('Sending request for media segment ...');
  xhr.send();
}

function onSourceOpen() {
  console.log("Source Open. This state:", this.readyState); // open
  const mediaSource = this;
  const sourceBuffer = mediaSource.addSourceBuffer(mimeCodec);
  sourceBuffer.memoryLimit = 300 * 1024 * 1024;
  // mem is 31,457,280 // appendCount 85

  let MIN_SIZE = 12 * 1024 * 1024;
  let ESTIMATED_MIN_TIME = 12;

  fetchAB(assetURL, (buf) => {
    let expectedTime = 0;
    let expectedSize = 0;
    let appendCount = 0;


    let onBufferFull = function() {
      console.log("ON BUFFER FULL! appendCount:" + appendCount + " expectedTime:" + expectedTime);
      //if (expectedTime - sourceBuffer.buffer.start(0) < ESTIMATED_MIN_TIME) {
      //}
    }

    sourceBuffer.addEventListener("updateend", function onupdateend() {
      console.log("Update end. State is " + sourceBuffer.updating);
      appendCount++;
      console.log("Append Count" + appendCount);
      console.log("BUFFERD START:" + sourceBuffer.buffered.start(0) + " END:" + sourceBuffer.buffered.end(0));
      if (sourceBuffer.buffered.start(0) > 0 || expectedTime > sourceBuffer.buffered.end(0)) {
         console.log("OK< WE GOOD, LETS END!");
         sourceBuffer.removeEventListener('updatedend', onupdateend);
         onBufferFull();
      } else {
        console.log("OK< WE ELSE!");
        expectedTime +=  assetDuration;
        expectedSize += assetSize;
        console.log("SIZE:" + expectedSize + " TIME:" +  expectedTime);
        if (expectedSize > (10 * MIN_SIZE)) {
          console.log("EXPECTED SIZE IS > 10 * MIN SIZ!!");
          sourceBuffer.removeEventListener('updateend', onupdateend);
          onBufferFull();
          return;
        }
        console.log("aIIGHT, SOURCEBUG:", sourceBuffer);
        console.log("aIIGHT, OFFSET:", sourceBuffer.timestampOffset);
        try {
          console.log("Updating is " + sourceBuffer.updating);
          sourceBuffer.timestampOffset = expectedTime;
        } catch(e) {
          console.log("OUCH! = " + e);
          console.log(sourceBuffer);
        }
        console.log("aIIGHT, SB tsOffset is now " , expectedTime);
        try {
          console.log("AIIIGHT< LETS TRY TO APPEND AGAIN!");
          console.log("MEMORY SIZE:" + sourceBuffer.memoryLimit);
          sourceBuffer.appendBuffer(buf);
        } catch(e) {
          console.log("WUFF!");
          if (e.code == QUOTA_EXCEEDED_ERROR_CODE) {
            sourceBuffer.removeEventListener('updateend', onupdateend);
            onBufferFull();
          } else {
            console.log("DIFFERENTE RRRO! " + e);
          }
        }
      }
    });

    console.log("FIRST APPEND!");
    sourceBuffer.appendBuffer(buf);
  });
}


function createMediaSource() {

  console.log('Video Get Element By Id...');
  var video = document.getElementById('video');

  console.log('Create Media Source...');
  var mediaSource = new MediaSource;

  console.log('Attaching MediaSource to video element ...');
  video.src = window.URL.createObjectURL(mediaSource);

  console.log('Add event listener..');
  mediaSource.addEventListener('sourceopen', onSourceOpen);

}

addEventListener('load', createMediaSource);
