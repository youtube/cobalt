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

const VIDEO_CONTENT_TYPE =
  'video/mp4; codecs="avc1.4d002a"; width=1280; height=720';
const VIDEO_URL = 'BODe_h264_720p.mp4';

const PLAYBACK_DURATION_IN_SECONDS = 5;
const MAX_DURATION_IN_SECONDS = 15;

class VideoContext {
  videoElement;
  videoId;
  currentOffset = 0;
  audioContentType;
  audioSegment;
  videoSegment;
  playStartTime = 0;
  finished = false;
  constructor() {}
}

function getAudioContentType(codec) {
  if (codec === "aac") {
    return 'audio/mp4; codecs="mp4a.40.2"';
  } else if (codec === "opus") {
    return 'audio/webm; codecs="opus"';
  } else if (codec === "ac3") {
    return 'audio/mp4; codecs="ac-3"';
  } else {
    throw "Invalid codec: " + codec;
  }
}

function getAudioContentFilename(codec) {
  if (codec === "aac") {
    return 'BODe_aac_140.mp4';
  } else if (codec === "opus") {
    return 'BODe_opus_251.webm';
  } else if (codec === "ac3") {
    return 'BODe_ac3.mp4';
  } else {
    throw "Invalid codec: " + codec;
  }
}

function fetchArrayBuffer(url, cb) {
  console.log('called fetchArrayBuffer');
  var xhr = new XMLHttpRequest();
  xhr.responseType = 'arraybuffer';
  xhr.addEventListener('load', function() {
    cb(xhr.response);
  });
  xhr.open('GET', url);
  xhr.send();
}

function swapVideo(currentVideo, waitingVideo) {
  waitingVideo.classList.remove('current-video');
  currentVideo.classList.add('current-video');
}

async function prepareVideo(videoId, videoContext, codec, onContextCreated) {
  let videoElement = document.getElementById(videoId);

  let audioContentType = getAudioContentType(codec);
  fetchArrayBuffer(VIDEO_URL,
    function(videoArrayBuffer) {
      console.log("After fetching video buffer");
      fetchArrayBuffer(getAudioContentFilename(codec),
        function(audioArrayBuffer) {
          console.log("After fetching audio buffer");
          videoContext.videoElement = videoElement;
          videoContext.videoId = videoId;
          videoContext.audioContentType = audioContentType;
          videoContext.audioSegment = audioArrayBuffer;
          videoContext.videoSegment = videoArrayBuffer;
          onContextCreated();
        });
    });
}

function playVideoSegment(currentVideo, waitingVideo, mediaSource) {
  console.log("Called playVideoSegment() hmm.");
  console.log(currentVideo.currentOffset);

  console.log("Creating new mediasource.");
  console.log(mediaSource);
  currentVideo.videoElement.load();
  currentVideo.videoElement.src = URL.createObjectURL(mediaSource);

  mediaSource.addEventListener('sourceopen', function() {
    console.log("Media source is open: " + String(mediaSource.readyState));
    let audioContentType = currentVideo.audioContentType;
    console.log(mediaSource.sourceBuffers.length);
    let audioSourceBuffer = mediaSource.addSourceBuffer(audioContentType);
    let videoSourceBuffer = mediaSource.addSourceBuffer(VIDEO_CONTENT_TYPE);
    console.log(mediaSource.sourceBuffers.length);

    audioSourceBuffer.appendWindowEnd = currentVideo.currentOffset +
      PLAYBACK_DURATION_IN_SECONDS;
    videoSourceBuffer.appendWindowEnd = currentVideo.currentOffset +
      PLAYBACK_DURATION_IN_SECONDS;
    if (currentVideo.currentOffset + PLAYBACK_DURATION_IN_SECONDS >
      MAX_DURATION_IN_SECONDS) {
      audioSourceBuffer.appendWindowEnd = MAX_DURATION_IN_SECONDS;
      videoSourceBuffer.appendWindowEnd = MAX_DURATION_IN_SECONDS;
    }

    videoSourceBuffer.addEventListener("updateend",
      function onvideoupdateend() {
        console.log("Appended video data.");
        audioSourceBuffer.addEventListener("updateend",
          function onaudioupdateend() {
            mediaSource.endOfStream();
            audioSourceBuffer.removeEventListener('updateend',
              onvideoupdateend);
            console.log("Appended audio data.");
            console.log("Updating current offset: " + String(
              currentVideo.currentOffset) + " to: " + String(
              currentVideo.currentOffset +
              PLAYBACK_DURATION_IN_SECONDS));
            if (currentVideo.currentOffset +
              PLAYBACK_DURATION_IN_SECONDS >= MAX_DURATION_IN_SECONDS) {
              console.log("Appended last segment for video " +
                currentVideo.videoId);
              currentVideo.finished = true;
            }

            function onended() {
              console.log("Ended segment.");
              console.log('video.currentTime after ending is ' +
                currentVideo.videoElement.currentTime);
              // Swap to waiting video if playback hasn't finished.
              if (waitingVideo.finished === false) {
                console.log('new vid time');
                // Wait until after the current video suspends playback to begin
                // new playback.
                let waitTime = Math.max((currentVideo.playStartTime + (
                    PLAYBACK_DURATION_IN_SECONDS * 1000)) - Date
                .now(), 0) + 750;
                setTimeout(function() {
                  playVideoSegment(waitingVideo, currentVideo,
                    new MediaSource);
                }, waitTime);
              } else {
                console.log("Video " + waitingVideo.videoId +
                  " is finished.");
              }
            }

            ["ended"].forEach(function(event) {
              currentVideo.videoElement.addEventListener(event,
                onended);
            });

            console.log("Playing video " + currentVideo.videoId);
            swapVideo(currentVideo.videoElement, waitingVideo
              .videoElement);
            currentVideo.videoElement.currentTime = currentVideo
              .currentOffset;
            console.log('video.currentTime is ' + currentVideo
              .videoElement.currentTime);
            currentVideo.videoElement.play();
            currentVideo.playStartTime = Date.now();
            currentVideo.currentOffset = currentVideo.currentOffset +
              PLAYBACK_DURATION_IN_SECONDS;
          });

        audioSourceBuffer.appendWindowStart = currentVideo.currentOffset;
        audioSourceBuffer.timestampOffset = currentVideo.currentOffset;
        console.log("Start audio appendBuffer().");
        audioSourceBuffer.appendBuffer(currentVideo.audioSegment);
      });

    console.log("Start video appendBuffer().");
    console.log(currentVideo.videoSegment);
    videoSourceBuffer.appendBuffer(currentVideo.videoSegment);
  });
}

function playVideos(codec) {
  console.log("Playing with codec: " + codec);
  let video1 = new VideoContext;
  let video2 = new VideoContext;
  prepareVideo("video-1", video1, codec, function() {
    prepareVideo("video-2", video2, codec, function() {
      playVideoSegment(video1, video2, new MediaSource);
    });
  });
}

function changeCodecSelection(codecList, currentIndex, newIndex) {
  codecList[currentIndex].classList.remove("chosen-codec");
  codecList[newIndex].classList.add("chosen-codec");
}

function getListIndex(currentIndex, size, delta) {
  let newIndex = currentIndex + delta;
  if (newIndex < 0) {
    newIndex = size - 1;
  } else if (newIndex >= size) {
    newIndex = 0;
  }

  return newIndex;
}

function waitforKeyPress() {
  let opus = document.getElementById("opus");
  let aac = document.getElementById("aac");
  let ac3 = document.getElementById("ac3");

  let codecSelection = [opus, aac, ac3];
  let currentIndex = 0;
  document.onkeydown = function(e) {
    console.log("key down with code: " + String(e.keyCode));
    let newIndex = currentIndex;
    switch (e.keyCode) {
      case 13:
      case 32768:
        // Enter, Android enter.
        document.onkeydown = null;
        document.getElementById("codec-selection-area").outerHTML = "";
        document.getElementById("videos").hidden = false;
        playVideos(codecSelection[currentIndex].id);
        break;
      case 37:
      case 38:
      case 32782:
      case 32780:
        // Left, up, Android left, up.
        newIndex = getListIndex(currentIndex, codecSelection.length, -1);
        break;
      case 39:
      case 40:
      case 32781:
      case 32783:
        // Right, down, Android right, down.
        newIndex = getListIndex(currentIndex, codecSelection.length, 1);
        break;
      default:
        return;
    }
    changeCodecSelection(codecSelection, currentIndex, newIndex);
    currentIndex = newIndex;
  };
}

addEventListener('load', waitforKeyPress);
