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

var nextVideoElementIndex = 0;
var audioData;
var videoData;

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
    var videoBuffer = ms.addSourceBuffer('video/webm; codecs="vp9"; decode-to-texture=true');
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

function setupKeyHandler() {
  document.onkeydown = function() {
    videoElements = document.getElementsByTagName('video');
    for(let i = 0; i < videoElements.length; i++) {
      if (videoElements[i].playing) {
        console.log("Ignore key press as a video is still playing.");
        return;
      }
    }

    nextVideoElementIndex = nextVideoElementIndex % videoElements.length;

    console.log("Trying to play next video at index " + nextVideoElementIndex);

    var currentVideoElement = videoElements[nextVideoElementIndex];
    if (currentVideoElement.setMaxVideoCapabilities) {
      if (nextVideoElementIndex < videoElements.length / 2) {
        currentVideoElement.setMaxVideoCapabilities("");
      } else {
        currentVideoElement.setMaxVideoCapabilities("width=1920; height=1080");
      }
    }

    nextVideoElementIndex++;

    playVideoOn(currentVideoElement);
  };
}

downloadMediaData(setupKeyHandler);
