// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// The page simply plays an audio or a video stream in a loop, it can be used
// in the following forms:
//   loop-playback.html?url=video.mp4&type=progressive
//   loop-playback.html?url=video.webm&type=video
//   loop-playback.html?url=audio.mp4&type=audio
// If the stream is adaptive, it has to be fit in memory as this demo will
// download the whole stream at once.

var video = null;
var url = null;
var type = null;

function downloadAndAppend(source_buffer, callback) {
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

function onVideoEnded() {
  console.log('playback ended');
  startNextVideo();
}

function startProgressiveVideo() {
  video.src = '';
  video.load();
  video.src = url;
  video.addEventListener('ended', onVideoEnded.bind());
}

function startAdaptiveVideo() {
  video.src = '';
  video.load();
  var mediasource = new MediaSource;
  mediasource.addEventListener('sourceopen', function () {
    var source_buffer;
    if (type == "audio") {
      if (url.indexOf(".mp4") != -1) {
        source_buffer = mediasource.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
      } else if (url.indexOf(".webm") != -1) {
        source_buffer = mediasource.addSourceBuffer('audio/webm; codecs="opus"');
      } else {
        throw "unknown audio format " + url;
      }
    } else {
      if (url.indexOf(".mp4") != -1) {
        source_buffer = mediasource.addSourceBuffer('video/mp4; codecs="avc1.640028"');
      } else if (url.indexOf(".webm") != -1) {
        source_buffer = mediasource.addSourceBuffer('video/webm; codecs="vp9"');
      } else {
        throw "unknown video format " + url;
      }
    }
    downloadAndAppend(source_buffer, function () {
      mediasource.endOfStream();
    });
  })

  video.src = window.URL.createObjectURL(mediasource);
  video.addEventListener('ended', onVideoEnded);
}

function startNextVideo() {
  if (type == "progressive") {
    startProgressiveVideo();
  } else {
    startAdaptiveVideo();
  }
}

function main() {
  var get_parameters = window.location.search.substr(1).split('&');
  for (var param of get_parameters) {
    splitted = param.split('=');
    if (splitted[0] == 'url') {
      url = splitted[1];
    } else if (splitted[0] == 'type') {
      type = splitted[1];
    }
  }

  if (!url) {
    throw "url is not set.";
  }

  if (type != 'progressive' && type != 'audio' && type != 'video') {
    throw "invalid type " + type;
  }

  video = createVideoElement();
  startNextVideo();
}

main();
