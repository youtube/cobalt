// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

const video = document.getElementById('video'); // This is the Ad video which goes into PiP
const mainVideo = document.getElementById('main-video'); // This is Big Buck Bunny
const logDiv = document.getElementById('log');

function log(msg) {
  console.log('JS: ' + msg);
}

log('Page loaded');

// Load Main Video via MSE in Overlay Mode (without decode-to-texture=true)
function loadMainVideoViaMSE() {
  if (typeof MediaSource === 'undefined') return;
  const mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', async () => {
    try {
      const sourceBuffer = mediaSource.addSourceBuffer('video/webm; codecs="vp9"');
      const fetchUrl = 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/big-buck-bunny-vp9-480p-30fps.webm';
      // const fetchUrl = 'https://storage.googleapis.com/shaka-demo-assets/sintel-webm-only/v-0720p-1800k-vp9.webm';
      log('Fetching Main Video (Overlay): ' + fetchUrl);
      const response = await fetch(fetchUrl, { headers: { 'Range': 'bytes=0-5000000' } });
      if (!response.ok) {
        throw new Error('Network response was not ok: ' + fetchUrl);
      }
      const data = await response.arrayBuffer();

      sourceBuffer.addEventListener('updateend', () => {
        if (!sourceBuffer.updating && mediaSource.readyState === 'open') {
          mediaSource.endOfStream();
          log('MSE Stream ready for Main Video (Overlay).');
          mainVideo.play().catch(e => log('BBB play prevented: ' + e));
        }
      }, { once: true });
      sourceBuffer.appendBuffer(data);
    } catch (error) {
      log('Error loading Main Video: ' + error);
    }
  }, { once: true });
  mainVideo.src = URL.createObjectURL(mediaSource);
  mainVideo.loop = true;
}

loadMainVideoViaMSE();

// Load Ad Video via MSE just like the original logic (in texture mode)
function loadAdVideoViaMSE() {
  if (typeof MediaSource === 'undefined') {
    log('MediaSource is not supported.');
    return;
  }
  const mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', async () => {
    URL.revokeObjectURL(video.src);
    try {
      // PiP is only supported in decode-to-texture mode
      const sourceBuffer = mediaSource.addSourceBuffer('video/webm; codecs="vp9"; decode-to-texture=true');
      // const fetchUrl = '../configure-max-video-input-size/vp9-720p.webm';
      const fetchUrl = 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/vp9-live-1080p-30fps.webm';
      log('Fetching Ad: ' + fetchUrl);
      const response = await fetch(fetchUrl);
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      const data = await response.arrayBuffer();

      sourceBuffer.addEventListener('updateend', () => {
        if (!sourceBuffer.updating && mediaSource.readyState === 'open') {
          mediaSource.endOfStream();
          log('MSE Stream ready for Ad.');
        }
      }, { once: true });
      sourceBuffer.appendBuffer(data);
    } catch (error) {
      log('Error loading Ad video: ' + error);
    }
  }, { once: true });
  video.src = URL.createObjectURL(mediaSource);
}

// Automatically exiting PiP is natively allowed without a gesture
video.addEventListener('ended', async () => {
  log('Ad video ended.');
  try {
    log('Auto-exiting PiP due to end of Ad...');
    if (document.pictureInPictureElement) {
      await document.exitPictureInPicture();
    }
  } catch (e) {
    log('Error exiting PiP: ' + e);
  }
  const adTakeover = document.getElementById('ad-takeover-layer');
  if (adTakeover) {
    adTakeover.style.display = 'none';
    adTakeover.style.background = 'black';
    adTakeover.style.left = '0px';
    adTakeover.style.pointerEvents = 'auto';
  }
  minimizeBtn.classList.remove('visible');
});

// PiP Event Listeners for logging and cleanup
video.addEventListener('enterpictureinpicture', () => {
  log('Entered PiP Successfully. time=' + Math.round(performance.now()));

  // Immediately hide the video element to hide the native "Playing in picture-in-picture" placeholder
  video.style.opacity = '0';

  // The JS enterpictureinpicture event fires as soon as the PiP mode is acknowledged,
  // BUT the Android OS WindowManager is still physically animating the window into the corner!
  // We wait 500ms to guarantee the animation has settled!
  setTimeout(() => {

    // Restore opacity now that it is off-screen, so it's ready for the next playback
    video.style.opacity = '1';

    // The Ad is now floating! We must hide the DOM takeover layer so BBB can be seen again in the background.
    const adTakeover = document.getElementById('ad-takeover-layer');
    if (adTakeover) {
      adTakeover.style.left = '-3000px';
      adTakeover.style.pointerEvents = 'none';
    }
  }, 500);
});

const minimizeBtn = document.getElementById('minimize-btn');

minimizeBtn.addEventListener('click', () => {
  log('User clicked minimize! time=' + Math.round(performance.now()));

  // Make the takeover layer transparent
  const adTakeover = document.getElementById('ad-takeover-layer');
  if (adTakeover) {
    adTakeover.style.background = 'transparent';
  }

  // Immediately hide the button
  minimizeBtn.style.display = 'none';

  // Request PiP immediately (no delay needed as we don't change layout)
  video.requestPictureInPicture().catch(err => {
    log('PIP ERROR: ' + err);
  });
});

// Bind physical remote "Down Arrow" to this same action for convenience
window.addEventListener('keydown', (e) => {
  if (e.key === 'ArrowDown' || e.keyCode === 40) {
    if (document.getElementById('ad-takeover-layer').style.display === 'flex' &&
        minimizeBtn.classList.contains('visible')) {
      log('Remote ArrowDown pressed, triggering minimize...');

      // Visually simulate the remote button press on our UI button
      minimizeBtn.style.transform = 'scale(0.9)';
      minimizeBtn.style.backgroundColor = 'lightgray';
      setTimeout(() => {
        minimizeBtn.style.transform = 'scale(1.1)';
        minimizeBtn.style.backgroundColor = 'white';
        minimizeBtn.click();
      }, 150);
    }
  }
});

const noticeOverlay = document.getElementById('notice-overlay');
if (noticeOverlay) noticeOverlay.style.display = 'none';

mainVideo.addEventListener('playing', () => {
  log('BBB is now playing. Pre-loading the Ad silently in the background...');
  // Load the ad now that the main video has successfully claimed initial bandwidth!
  loadAdVideoViaMSE();

  log('Enjoy 5 seconds of movie before the Ad notice...');

  setTimeout(() => {
    log('Showing Ad Notice...');
    if (noticeOverlay) noticeOverlay.style.display = 'block';

    let countdown = 5;
    const countdownSpan = document.getElementById('countdown');
    const adTakeover = document.getElementById('ad-takeover-layer');

    const intervalId = setInterval(() => {
      countdown--;
      if (countdownSpan) {
        countdownSpan.textContent = countdown.toString();
      }

      if (countdown <= 0) {
        clearInterval(intervalId);
        log('Triggering Ad Takeover...');
        if (noticeOverlay) noticeOverlay.style.display = 'none';
        if (adTakeover) {
          adTakeover.style.display = 'flex';
        }
        video.play().catch(e => log('Ad play prevented: ' + e));

        // Let the Ad play in full screen for 1 second, then fade in the button and focus it
        setTimeout(() => {
          if (adTakeover && adTakeover.style.display === 'flex') {
            minimizeBtn.style.display = 'flex'; // Restore display if hidden by previous click
            minimizeBtn.classList.add('visible');
            minimizeBtn.focus();
            log('Button faded in and focused.');
          }
        }, 1000);
      }
    }, 1000);
  }, 5000); // Play BBB flawlessly for 5s first
}, { once: true });
