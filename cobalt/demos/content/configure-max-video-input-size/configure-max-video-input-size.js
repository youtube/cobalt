// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

var audioData;
var videoData;

let status_div;

function downloadMediaData(downloadedCallback) {
  var xhr = new XMLHttpRequest;

  xhr.onload = function() {
    audioData = xhr.response;
    console.log("Downloaded " + audioData.byteLength + " of audio data.");

    xhr.onload = function() {
      videoData = xhr.response;
      console.log("Downloaded " + videoData.byteLength + " of video data.");
      downloadedCallback();
    }

    xhr.open("GET", "vp9-720p.webm", true);
    xhr.send();
  }

  xhr.open("GET", "dash-audio.mp4", true);
  xhr.responseType = "arraybuffer";
  xhr.send();
}

function playVideoOn(videoElement) {
  var ms = new MediaSource;
  ms.addEventListener('sourceopen', function() {
    console.log("Creating SourceBuffer objects.");
    var audioBuffer = ms.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
    var videoBuffer = ms.addSourceBuffer('video/webm; codecs="vp9"');
    audioBuffer.addEventListener("updateend", function() {
      audioBuffer.abort();
      videoBuffer.addEventListener("updateend", function() {
        setTimeout(function() {
          videoBuffer.addEventListener("updateend", function() {
            videoBuffer.abort();
            ms.endOfStream();
            videoElement.ontimeupdate = function() {
              if (videoElement.currentTime > 10) {
                console.log("Stop playback.");
                videoElement.src = '';
                videoElement.load();
                videoElement.ontimeupdate = null;
              }
            }
            console.log("Start playback.");
            videoElement.play();
          });
          videoBuffer.appendBuffer(videoData.slice(1024));
        }, 5000);
      });
      videoBuffer.appendBuffer(videoData.slice(0, 1024));
    });
    audioBuffer.appendBuffer(audioData);
  });

  console.log("Attaching MediaSource to video element.");
  videoElement.src = URL.createObjectURL(ms);
}

function setupPlayerHandler() {
  // Setup one primary player and one sub player.
  // i = 0: primary player, buffer size is 3110500
  // i = 1: sub player, buffer size is 777000
  const maxVideoInputSizes = [3110500, 777000];
  videoElements = document.getElementsByTagName('video');
  status_div = document.getElementById('status');
  status_div.innerHTML += 'Video Get Element By Id...<br>';
  for(let i = 0; i < maxVideoInputSizes.length; i++) {
    if (videoElements[i].setMaxVideoInputSize && i < videoElements.length) {
      videoElements[i].setMaxVideoInputSize(maxVideoInputSizes[i]);
      status_div.innerHTML += 'Set Max Video Input Size of Video Element '+ i + ' to '+ maxVideoInputSizes[i] + '<br>';
    }
    if (i == 1 && videoElements[i].setMaxVideoCapabilities) {
      videoElements[i].setMaxVideoCapabilities("width=1920; height=1080");
    }
    playVideoOn(videoElements[i]);
  }
}

downloadMediaData(setupPlayerHandler);
