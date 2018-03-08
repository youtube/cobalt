// Copyright 2015 Google Inc. All Rights Reserved.
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

// The demo does the following steps in a loop:
//  1. Playback a progressive video.
//  2. Playback a section of a 240p video in adaptive (DASH).
//  3. Playback the above 240p video in 1080p.
// All above videos have audio accompanied.

// Description of the streams being played.
var kProgressiveVideoUrl = 'progressive.mp4';

var kAdaptiveVideoUrl_240p = 'dash-video-240p.mp4';
var kAdaptiveVideoSize_240p = 2051611;
// The size of the video that can be played for 10s.
var kAdaptiveVideo10sChunkSize_240p = 306689;

var kAdaptiveVideoUrl_1080p = 'dash-video-1080p.mp4';
var kAdaptiveVideoSize_1080p = 22461669;

var kAdaptiveAudioUrl = 'dash-audio.mp4';
var kAdaptiveAudioSize = 1048531;
var kAdaptiveAudioChunkSize = 720 * 1024;

var is_progressive = false;
var video = null;

// Send a range request of [`begin`, `end`] (inclusive) to download content from
// the `url` and append the downloaded content into the `source_buffer` before
// calling the `callback` function.
function downloadAndAppend(url, begin, end, source_buffer, callback) {
  var xhr = new XMLHttpRequest;
  xhr.open('GET', url, true);
  xhr.responseType = 'arraybuffer';
  xhr.addEventListener('load', function(e) {
    var onupdateend = function() {
      source_buffer.removeEventListener('updateend', onupdateend);
      callback();
    };
    source_buffer.addEventListener('updateend', onupdateend);
    source_buffer.appendBuffer(new Uint8Array(e.target.response));
  });
  xhr.setRequestHeader('Range', ('bytes=' + begin +'-' + end));
  xhr.send();
}

function createVideoElement() {
  var video = document.createElement('video');
  video.autoplay = true;
  video.style.width = '1280px';
  video.style.height = '720px';
  document.body.appendChild(video);

  return video;
}

function createStatusElement(video) {
  var status = document.createElement('div');
  document.body.appendChild(status);
  video.addEventListener('timeupdate', function () {
    status.textContent = is_progressive ? 'Progressive' : 'Adaptive';
    status.textContent += ' / time: ' + video.currentTime.toFixed(2);
  });

  return status;
}

function onVideoEnded() {
  console.log('ended');
  is_progressive = !is_progressive;
  startNextVideo();
}

function startProgressiveVideo(video) {
  video.src = '';
  video.load();
  video.src = kProgressiveVideoUrl;
  video.addEventListener('ended', onVideoEnded);
}

function startAdaptiveVideo(video) {
  video.src = '';
  video.load();
  var mediasource = new MediaSource;
  mediasource.addEventListener('sourceopen', function () {
    var video_source_buffer = mediasource.addSourceBuffer('video/mp4; codecs="avc1.640028"');
    var audio_source_buffer = mediasource.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
    downloadAndAppend('dash-video-1080p.mp4', 0, 15 * 1024 * 1024, video_source_buffer, function () {
      video_source_buffer.abort();
      // Append the first two segments of the 240p video so we can see the transition.
      downloadAndAppend('dash-video-240p.mp4', 0, kAdaptiveVideo10sChunkSize_240p, video_source_buffer, function () {
        video_source_buffer.abort();
        downloadAndAppend('dash-audio.mp4', 0, kAdaptiveAudioChunkSize, audio_source_buffer, function () {
          mediasource.endOfStream();
        });
      });
    });
  })

  video.src = window.URL.createObjectURL(mediasource);
  video.addEventListener('ended', onVideoEnded);
}

function startNextVideo() {
  if (is_progressive) {
    startProgressiveVideo(video);
  } else {
    startAdaptiveVideo(video);
  }
}

video = createVideoElement();
createStatusElement(video);
startNextVideo(video);
