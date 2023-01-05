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

function getFmp4AacData(onDataReady) {
  const FILE_NAME = 'fmp4-aac-44100-tiny.mp4';

  var xhr = new XMLHttpRequest;
  xhr.responseType = 'arraybuffer';
  xhr.addEventListener('readystatechange', function onreadystatechange() {
    if (xhr.readyState == XMLHttpRequest.DONE) {
      xhr.removeEventListener('readystatechange', onreadystatechange);

      console.log('Media segment downloaded.');
      onDataReady(xhr.response);
    }
  });
  xhr.open('GET', FILE_NAME, true);
  console.log('Sending request for media segment ...');
  xhr.send();
}

function createAndAttachMediaSource(onSourceOpen) {
  var video = document.getElementById('video');

  var mediaSource = new MediaSource;
  mediaSource.addEventListener('sourceopen', onSourceOpen);

  console.log('Attaching MediaSource to video element ...');
  video.src = window.URL.createObjectURL(mediaSource);
}

// Audio access units often contain more than one frame (in this case 1024
// frames).
// The function creates an audio stream with AUs that not all frames should be
// played, like:
//   ... [*AA*] [*BB*] [*CC*] [*DD*] [*EE*] ...
// 1. Each character represent ~256 frames.
// 2. The letter represents the AU index, i.e. C is the next AU of B.
// 3. '*' means the 256 frames are excluded from playback.
function appendMediaSegment(mediaSource, sourceBuffer, mediaSegment,
                            currentOffset) {
  // The input data is 44100hz, and each AU (access unit) contains 1024 frames.
  var HALF_AU_DURATION_IN_SECONDS = 1024.0 / 44100 / 2;
  var MAX_DURATION_IN_SECONDS = 5.;

  if (!currentOffset) {
    currentOffset = 0.0;
  }

  sourceBuffer.appendWindowEnd = MAX_DURATION_IN_SECONDS;

  sourceBuffer.addEventListener('updateend', function onupdateend() {
    sourceBuffer.removeEventListener('updateend', onupdateend);
    sourceBuffer.abort();

    currentOffset += HALF_AU_DURATION_IN_SECONDS;

    if (currentOffset < MAX_DURATION_IN_SECONDS) {
      appendMediaSegment(mediaSource, sourceBuffer, mediaSegment,
                         currentOffset);
    } else {
      mediaSource.endOfStream();
      console.log('video.currentTime is ?');
      var video = document.getElementById('video');
      console.log('video.currentTime is ' + video.currentTime);
      video.currentTime = HALF_AU_DURATION_IN_SECONDS / 2;
      video.play();
    }
  });

  console.log('Set timestampOffset to ' + currentOffset + ' before appending.');
  // Assuming the buffered AUs are
  // ... [*AAA] [BBBB] [CCCC] [DDDD] [EEEE] ...
  //
  // We setup the append by shifting `currentOffset` for 1/2 of an AU (so it
  // points to the middle of the AU), and `appendWindowStart` for 1/4 of AU
  // after `currentOffset`:
  //  currentOffset
  //        v
  // ... [*A A A] [BBBB] [CCCC] [DDDD] [EEEE] ...
  //          ^
  //   appendWindowStart
  //
  // So the new append will start from `currentOffset`, but the first 256 frames
  // will be masked due to `appendWindowStart`.  The result will be:
  // ... [*AA*] (all remaining AUs get replaced)
  // ...   [*XXX] [YYYY] [ZZZZ] ...
  // i.e.
  // ... [*AA*] [*XXX] [YYYY] [ZZZZ] ...
  // This results an AU with first and last 256 frames (out of 1024 frames)
  // excluded from playback.  A non-conforming implementation will play the
  // whole AU which takes twice of the time needed.
  sourceBuffer.timestampOffset = currentOffset;
  var appendWindowStart = currentOffset + HALF_AU_DURATION_IN_SECONDS / 2;
  if (currentOffset > 0 && appendWindowStart < sourceBuffer.appendWindowEnd) {
    sourceBuffer.appendWindowStart = appendWindowStart;
  }
  sourceBuffer.appendBuffer(mediaSegment);
}

function playPartialAudio() {
  window.setInterval(function() {
    document.getElementById('status').textContent =
        'currentTime ' + document.getElementById('video').currentTime;
  }, 100);

  getFmp4AacData(function(mediaSegment) {
    createAndAttachMediaSource(function(event) {
      var mediaSource = event.target;

      console.log('Adding SourceBuffer ...');
      var sourceBuffer =
          mediaSource.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');

      appendMediaSegment(mediaSource, sourceBuffer, mediaSegment);
    });
  });
}

addEventListener('load', playPartialAudio);
