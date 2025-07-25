// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const RECORD_FRAMES = 5
var srcVideo;
var dstVideo;
let recorder = null;
let stream = null;

// Note: This mime type is from the repro case for crbug.com/1360531
// It is not clear whether it would be worthwhile to run this test
// using a multitude of format/codec combinations.
const mimeType = 'video/webm;codecs=vp8';

const chunks = [];

function logOutput(s) {
  if (window.domAutomationController) {
    window.domAutomationController.log(s);
  } else {
    console.log(s);
  }
}

function setVideoSize() {
  const width = '240';
  const height = '135';
  srcVideo.width = width;
  srcVideo.height = height;
  dstVideo.width = width;
  srcVideo.height = height;
}

function startPlayback() {
  logOutput('Preparing playback.');
  var blob = new Blob(chunks, { 'type': mimeType });
  var videoURL = window.URL.createObjectURL(blob);
  dstVideo.onended = function() {
    logOutput('Playback complete.');
    domAutomationController.send('SUCCESS');
  }
  dstVideo.onerror = e => {
    logOutput(`Test failed: ${e.message}`);
    abort = true;
    domAutomationController.send('FAIL');
  };
  dstVideo.src = videoURL;
  dstVideo.play();
  logOutput('Playback started.');
}

function startRecording() {
  stream = srcVideo.captureStream(30);
  recorder = new MediaRecorder(stream, { mimeType });
  recorder.onstop = startPlayback;
  recorder.ondataavailable = (e) => {
    chunks.push(e.data);
  };

  recorder.start();
  srcVideo.play();

  stopRecordingAfterXFrames(RECORD_FRAMES);

  logOutput('Recording started.');
}

function stopRecordingAfterXFrames(x) {
  if (x <= 0) {
    stopRecording();
  } else {
    logOutput(`${x} frame(s) remaining.`);
    srcVideo.requestVideoFrameCallback(()=>{
      stopRecordingAfterXFrames(x-1);
    })
  }
}

function stopRecording() {
  recorder.stop();
  srcVideo.pause();
  logOutput('Recording stopped.');
}

function main() {
  logOutput('Test started');
  srcVideo = document.getElementById('src-video');
  srcVideo.loop = true;
  srcVideo.muted = true;  // No need to exercise audio paths.
  dstVideo = document.getElementById('dst-video');
  dstVideo.loop = false;
  dstVideo.muted = true;  // No need to exercise audio paths.
  setVideoSize();

  srcVideo.onerror = e => {
    logOutput(`Test failed: ${e.message}`);
    abort = true;
    domAutomationController.send('FAIL');
  };
  srcVideo.requestVideoFrameCallback(startRecording);
}
