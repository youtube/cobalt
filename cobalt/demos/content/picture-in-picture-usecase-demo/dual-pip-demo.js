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

const v1 = document.getElementById('video1');
const v2 = document.getElementById('video2');
const v1Wrapper = document.getElementById('v1-wrap');
const v2Wrapper = document.getElementById('v2-wrap');


function log(msg) {
  console.log('JS: ' + msg);
}

const url1 = 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/big-buck-bunny-vp9-480p-30fps.webm';
const url2 = 'https://storage.googleapis.com/shaka-demo-assets/sintel-webm-only/v-0720p-1800k-vp9.webm';

function bootstrapVideo(videoObj, url, useTexture = false) {
  return new Promise((resolve) => {
    if (typeof MediaSource === 'undefined') { log('MSE not supported'); return resolve(); }
    const mediaSource = new MediaSource();
    mediaSource.addEventListener('sourceopen', async () => {
      try {
        const isMp4 = url.toLowerCase().includes('.mp4');
        const mimeType = isMp4 ? 'video/mp4; codecs="avc1.640028"' : 'video/webm; codecs="vp9"';
        const codecString = mimeType + (useTexture ? '; decode-to-texture=true' : '');
        const sourceBuffer = mediaSource.addSourceBuffer(codecString);
        // Safely fetch only the first 5MB of media to prevent JS Heap OOM
        // crashes when testing massive external videos on constrained TV memory
        const response = await fetch(url, { headers: { 'Range': 'bytes=0-5000000' } });
        if (!response.ok) {
          throw new Error(`Failed to fetch video: ${response.status} ${response.statusText}`);
        }
        const data = await response.arrayBuffer();
        sourceBuffer.addEventListener('updateend', () => {
           if (!sourceBuffer.updating && mediaSource.readyState === 'open') {
             mediaSource.endOfStream();
             videoObj.play().catch(e => log('Play prevented: ' + e));
             resolve();
           }
        }, { once: true });
        sourceBuffer.appendBuffer(data);
      } catch (error) { log('Error loading MSE: ' + error); resolve(); }
    }, { once: true });

    videoObj.src = URL.createObjectURL(mediaSource);
    videoObj.loop = true;
  });
}

let isStage3 = false;
let isTransitioning = false;

document.addEventListener('keydown', async (e) => {
  // Bind the down button to enter and exit pip
  if (e.key !== 'ArrowDown') return;
  if (isTransitioning) return;
  isTransitioning = true;

  try {
    if (!isStage3) {
      log('Stage 3: Native PiP for V1, Fullscreen for V2');

      try {
        if (typeof v1.requestPictureInPicture !== 'function') {
          throw new Error('Picture-in-Picture API is not supported on this device');
        }
        await v1.requestPictureInPicture();

        setTimeout(() => {
          v2Wrapper.style.transition = 'none';
          v2Wrapper.classList.add('fullscreen');

          // Restore transition shortly after jump is completed
          setTimeout(() => {
            v2Wrapper.style.transition = '';
          }, 100);
        }, 300);

        isStage3 = true;
      } catch (err) { log('Native PiP Error: ' + err.message); }

      isStage3 = true;
    } else {
      log('Stage 2: Side-by-Side (Exiting PiP)');

      v2Wrapper.classList.remove('fullscreen');
      try { await document.exitPictureInPicture(); } catch (err) {}

      isStage3 = false;
    }
  } catch(e) { log(e.message); }

  // Wait for the 500ms CSS transition to mathematically conclude before allowing another toggle
  setTimeout(() => { isTransitioning = false; }, 500);
});

log('Bootstrapping decoders...');
// Serialize loading to prevent Cobalt Thread/Compositor contention
// which causes the hardware overlay punch-out hole to silently drop
bootstrapVideo(v1, url1, true).then(() => {
  if (v1.readyState >= 3) {
    bootstrapVideo(v2, url2, false);
  } else {
    v1.addEventListener('playing', () => {
      bootstrapVideo(v2, url2, false);
    }, { once: true });
  }
});
