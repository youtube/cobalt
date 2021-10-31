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
//   background-mode-demo.html&type=video
//   background-mode-demo.html&type=audio
// If the stream is adaptive, it has to be fit in memory as this demo will
// download the whole stream at once.

var video = null;
var type = null;

let playlist = getPlaylist();
let index = 0;
let defaultSkipTime = 10;

var kAdaptiveAudioChunkSize = 720 * 1024;

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

function onVideoEnded() {
  startNextVideo();
}

function startAdaptiveVideo() {
  video.src = '';
  video.load();
  var mediasource = new MediaSource;
  mediasource.addEventListener('sourceopen', function () {
    if (type == 'audio') {
      var audio_source_buffer = mediasource.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
	downloadAndAppend('../media-element-demo/dash-audio.mp4', 0, kAdaptiveAudioChunkSize * 10, audio_source_buffer, function () {
        mediasource.endOfStream();
      });
    }

    if (type == 'video') {
      var video_source_buffer = mediasource.addSourceBuffer('video/mp4; codecs="avc1.640028"');
      var audio_source_buffer = mediasource.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
      downloadAndAppend('../media-element-demo/dash-video-1080p.mp4', 0, 15 * 1024 * 1024, video_source_buffer, function () {
        video_source_buffer.abort();
           // Append the first two segments of the 240p video so we can see the transition.
        downloadAndAppend('../media-element-demo/dash-audio.mp4', 0, kAdaptiveAudioChunkSize * 10, audio_source_buffer, function () {
          mediasource.endOfStream();
        });
      });
    }
  })

  video.src = window.URL.createObjectURL(mediasource);
  video.addEventListener('ended', onVideoEnded);
}


function startNextVideo() {
  startAdaptiveVideo();
}

function checkMediaType() {
  var get_parameters = window.location.search.substr(1).split('&');
  for (var param of get_parameters) {
    splitted = param.split('=');
    if (splitted[0] == 'type') {
      type = splitted[1];
    }
  }

  if (type != 'audio' && type != 'video') {
    throw "invalid type " + type;
  }
}

function main() {
  checkMediaType();
  video = createVideoElement();
  startNextVideo();
  setUpMediaSessionHandlers();
  updateMetadata();
}

// MediaSession
function updateMetadata() {
  let track = playlist[index];

  navigator.mediaSession.metadata = new MediaMetadata({
    title: track.title,
    artist: track.artist,
  });
  navigator.mediaSession.playbackState = "playing";
}

function setUpMediaSessionHandlers() {
  navigator.mediaSession.setActionHandler('seekbackward', function(event) {
    const skipTime = event.seekOffset || defaultSkipTime;
    video.currentTime = Math.max(video.currentTime - skipTime, 0);
    updatePositionState();
  });

  navigator.mediaSession.setActionHandler('seekforward', function(event) {
    const skipTime = event.seekOffset || defaultSkipTime;
    video.currentTime = Math.min(video.currentTime + skipTime, video.duration);
    updatePositionState();
  });

  navigator.mediaSession.setActionHandler('play', function() {
    log_info('TimeStamp: ' + getTime() + ' seconds' + ' play');
    video.play();
    navigator.mediaSession.playbackState = "playing";
  });

  navigator.mediaSession.setActionHandler('pause', function() {
    log_info('TimeStamp: ' + getTime() + ' seconds' + ' pause');
    video.pause();
    navigator.mediaSession.playbackState = "paused";
  });

  try {
    navigator.mediaSession.setActionHandler('stop', function() {
      log_info('TimeStamp: ' + getTime() + ' seconds' + ' stop');
    });
  } catch(error) {
  }

  try {
    navigator.mediaSession.setActionHandler('seekto', function(event) {
      if (event.fastSeek && ('fastSeek' in video)) {
        video.fastSeek(event.seekTime);
        return;
      }
      video.currentTime = event.seekTime;
      updatePositionState();
    });
  } catch(error) {
  }
}

function getPlaylist() {
  return [{title: 'Background mode demo', artist: 'Cobalt',}];
}

function log_info(message) {
  console.warn(message);
  document.getElementById('info').innerHTML += message + '.\n';
}

function getTime() {
  return Math.floor(Date.now() / 1000 | 0);
}

main();
