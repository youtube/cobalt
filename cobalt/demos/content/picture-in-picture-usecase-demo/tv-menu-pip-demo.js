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

function log(msg) {
  console.log('JS: ' + msg);
}

log('Page loaded. TV Menu PiP Demo (Strict Layout-First Decoder Bootstrapping)...');

// Section Data Configuration for Dynamic DOM Generation
const SECTIONS = [
  { title: 'MOVIES', startIndex: 0 },
  { title: 'SHORTS', startIndex: 3 },
  { title: 'LIVE', startIndex: 6 },
  { title: 'SERIES', startIndex: 9 }
];

const TOTAL_BLOCKS = 12;
const COLS = 3;
const ROWS = 4;
const blocks = [];

// Dynamically Render Catalog DOM Elements via Array Map
function renderCatalog() {
  const container = document.getElementById('catalog-container');
  if (!container) return;
  container.innerHTML = '';

  SECTIONS.forEach(sec => {
    const secEl = document.createElement('div');
    secEl.className = 'category-section';

    const titleEl = document.createElement('div');
    titleEl.className = 'category-title';
    titleEl.textContent = sec.title;
    secEl.appendChild(titleEl);

    const gridEl = document.createElement('div');
    gridEl.className = 'menu-grid';

    for (let i = 0; i < 3; i++) {
      const idx = sec.startIndex + i;
      const blockEl = document.createElement('div');
      blockEl.id = `block-${idx}`;
      blockEl.className = `video-block ${idx === 0 ? 'focused' : ''}`;

      if (idx === 0) {
        // Block 0: Big Buck Bunny (Texture Mode, PiP Enabled)
        blockEl.classList.add('texture-block');
        blockEl.innerHTML = `
          <video id="video-bbb-1" autoplay muted loop playsinline style="visibility: hidden;"></video>
          <div id="bbb-poster" class="bbb-poster">
            <div class="poster-text">PLAYING IN PIP</div>
          </div>
          <div class="focus-ring"></div>
        `;
      } else if (idx === 3) {
        // Block 3: Hardware Overlay Mode Video
        blockEl.innerHTML = `
          <video id="video-ad" autoplay muted loop playsinline style="visibility: hidden;"></video>
          <div class="overlay-mask"></div>
          <div class="focus-ring"></div>
        `;
      } else {
        // Placeholder Blocks (VIDEO 2 - 12)
        blockEl.classList.add('card-placeholder');
        blockEl.innerHTML = `
          <div class="card-content">
            <div class="card-title">VIDEO ${idx + 1}</div>
          </div>
          <div class="focus-ring"></div>
        `;
      }

      gridEl.appendChild(blockEl);
      blocks[idx] = blockEl;
    }

    secEl.appendChild(gridEl);
    container.appendChild(secEl);
  });
}

// Execute Dynamic Catalog Rendering
renderCatalog();

const vBbb1 = document.getElementById('video-bbb-1'); // Stream 1 (BBB) - Texture Mode with PiP
const vAd = document.getElementById('video-ad');       // Stream 4 (Block 3) - Hardware Overlay Mode
const bbbPoster = document.getElementById('bbb-poster');

const urlBBB = 'https://storage.googleapis.com/ytlr-cert.appspot.com/test-materials/media/big-buck-bunny-vp9-480p-30fps.webm';
const urlAd = '../configure-max-video-input-size/vp9-720p.webm';

function bootstrapVideo(videoObj, url, useTexture = false) {
  return new Promise((resolve) => {
    if (!videoObj || typeof MediaSource === 'undefined') {
      log('MediaSource not supported or video element missing');
      return resolve();
    }

    // Keep video element strictly hidden initially
    videoObj.style.visibility = 'hidden';

    // Wait until browser completes current animation frame layout calculation
    requestAnimationFrame(() => {
      // Force element layout rect calculation
      const rect = videoObj.getBoundingClientRect();
      log(`Bootstrapping video [${videoObj.id}], bounds: ${rect.width}x${rect.height} at (${rect.left}, ${rect.top})`);

      const mediaSource = new MediaSource();
      mediaSource.addEventListener('sourceopen', async () => {
        try {
          const isMp4 = url.toLowerCase().includes('.mp4');
          const mimeType = isMp4 ? 'video/mp4; codecs="avc1.640028"' : 'video/webm; codecs="vp9"';
          const codecString = mimeType + (useTexture ? '; decode-to-texture=true' : '');
          const sourceBuffer = mediaSource.addSourceBuffer(codecString);

          const response = await fetch(url, { headers: { 'Range': 'bytes=0-5000000' } });
          if (!response.ok) throw new Error(`Fetch failed: ${response.status}`);
          const data = await response.arrayBuffer();

          sourceBuffer.addEventListener('updateend', () => {
            if (!sourceBuffer.updating && mediaSource.readyState === 'open') {
              mediaSource.endOfStream();

              // Reveal video ONLY after layout is confirmed and first chunk is buffered
              requestAnimationFrame(() => {
                videoObj.style.visibility = 'visible';
                videoObj.play().catch(e => log('Play error: ' + e));
                resolve();
              });
            }
          }, { once: true });

          sourceBuffer.appendBuffer(data);
        } catch (err) {
          log('Load video error: ' + err);
          videoObj.style.visibility = 'visible';
          resolve();
        }
      }, { once: true });

      // Attach mediaSource URL after layout rect is confirmed
      videoObj.src = URL.createObjectURL(mediaSource);
      videoObj.loop = true;
    });
  });
}

let focusedIdx = 0;

function updateFocus(newIdx) {
  blocks.forEach(b => { if (b) b.classList.remove('focused'); });
  focusedIdx = Math.max(0, Math.min(TOTAL_BLOCKS - 1, newIdx));
  const el = blocks[focusedIdx];
  if (el) {
    el.classList.add('focused');
    const row = Math.floor(focusedIdx / COLS);
    const targetY = row * 270;

    window.scrollTo({ top: targetY, behavior: 'smooth' });
    document.documentElement.scrollTop = targetY;
    document.body.scrollTop = targetY;
  }
  log(`Focused Block [${focusedIdx}] (Y=${Math.floor(focusedIdx / COLS) * 270})`);
}

// Toggle PiP for v1 (Big Buck Bunny) via Center/OK button or Block 0 selection
async function toggleBbbPiP() {
  try {
    if (document.pictureInPictureElement) {
      log('Exiting Picture-in-Picture...');
      await document.exitPictureInPicture();
      log('Exited Picture-in-Picture!');
    } else {
      log('Entering Picture-in-Picture for v1 (BBB)...');
      if (typeof vBbb1.requestPictureInPicture === 'function') {
        await vBbb1.requestPictureInPicture();
        log('v1 (BBB) entered Picture-in-Picture successfully!');
      } else {
        log('PiP API not supported');
      }
    }
  } catch (err) {
    log('Toggle PiP Error: ' + err.message);
  }
}

// PiP Event Listeners for Cover Poster
if (vBbb1) {
  vBbb1.addEventListener('enterpictureinpicture', () => {
    log('v1 entered PiP: Showing placeholder poster at original slot...');
    if (bbbPoster) bbbPoster.style.display = 'flex';
  });

  vBbb1.addEventListener('leavepictureinpicture', () => {
    log('v1 exited PiP: Hiding placeholder poster...');
    if (bbbPoster) bbbPoster.style.display = 'none';
  });

  vBbb1.addEventListener('ended', async () => {
    log('Video ended. Exiting PiP...');
    if (document.pictureInPictureElement) {
      try { await document.exitPictureInPicture(); } catch (e) {}
    }
  });
}

// Click listener on Block 0 (v1 BBB)
if (document.getElementById('block-0')) {
  document.getElementById('block-0').addEventListener('click', () => {
    toggleBbbPiP();
  });
}

// Keyboard / Remote D-Pad Navigation & Selection Event Listener
window.addEventListener('keydown', (e) => {
  const row = Math.floor(focusedIdx / COLS);
  const col = focusedIdx % COLS;

  if (e.key === 'ArrowRight') {
    updateFocus(row * COLS + ((col + 1) % COLS));
  } else if (e.key === 'ArrowLeft') {
    updateFocus(row * COLS + ((col - 1 + COLS) % COLS));
  } else if (e.key === 'ArrowDown') {
    if (row < ROWS - 1) {
      updateFocus((row + 1) * COLS + col);
    }
  } else if (e.key === 'ArrowUp') {
    if (row > 0) {
      updateFocus((row - 1) * COLS + col);
    }
  } else if (e.key === 'Enter' || e.key === ' ' || e.keyCode === 13 || e.keyCode === 23) {
    log('Center/OK Button Pressed!');
    if (focusedIdx === 0 || document.pictureInPictureElement) {
      toggleBbbPiP();
    }
  }
});

// Bootstrapping Sequence: Deferred until initial DOM layout pass settles
function startBootstrapping() {
  log('Bootstrapping decoders after DOM layout pass...');
  bootstrapVideo(vBbb1, urlBBB, true).then(() => {
    bootstrapVideo(vAd, urlAd, false);
  });
}

if (document.readyState === 'complete' || document.readyState === 'interactive') {
  setTimeout(startBootstrapping, 50);
} else {
  window.addEventListener('DOMContentLoaded', () => {
    setTimeout(startBootstrapping, 50);
  });
}
