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

const mimeCodec = 'audio/mp4; codecs="mp4a.40.2"';
const assetURL = 'fmp4-aac-44100-tiny.mp4';

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
  sourceBuffer.memoryLimit = 1024;
  fetchAB(assetURL, (buf) => {
    sourceBuffer.addEventListener("updateend", () => {
      console.log("Update end..");
      console.log("MEMORY IS:", sourceBuffer);
      console.log("MEMORY IS:", sourceBuffer.memoryLimit);
      mediaSource.endOfStream();
      video.play();
      console.log(mediaSource.readyState); // ended
    });
    console.log("APPENDING..");
    sourceBuffer.appendBuffer(buf);
    // console.log("APPENDED..");
    // console.log("post APPEND MEMORY IS:", sourceBuffer.memoryLimit);
  });
  const sourceBuffer2 = mediaSource.addSourceBuffer(mimeCodec);
  sourceBuffer2.memoryLimit = 50 * 1024 * 1024;
  fetchAB(assetURL, (buf) => {
    sourceBuffer2.addEventListener("updateend", () => {
      console.log("Update end..");
      console.log("MEMORY IS:", sourceBuffer2);
      console.log("MEMORY IS:", sourceBuffer2.memoryLimit);
      mediaSource.endOfStream();
      video.play();
      console.log(mediaSource.readyState); // ended
    });
    console.log("APPENDING..");
    sourceBuffer2.appendBuffer(buf);
    // console.log("APPENDED..");
    // console.log("post APPEND MEMORY IS:", sourceBuffer.memoryLimit);
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
