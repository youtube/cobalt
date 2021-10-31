// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

(function() {

let ctx = window._mediaConsoleContext = {};

// NOTE: Place all "private" members and methods of |_mediaConsoleContext|
// below. Private functions are not attached to the |_mediaConsoleContext|
// directly. Rather they are referenced within the public functions, which
// prevent it from being garbage collected.

const kPlaybackRates = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0];

function getPrimaryVideo() {
  const elem = document.querySelectorAll('video');
  if (!elem || elem.length === 0) {
    return null;
  }

  let primary = null;
  for (let i = 0; i < elem.length; i++) {
    const rect = elem[i].getBoundingClientRect();
    if (rect.width > 0 && rect.height > 0 && !primary) {
      // When there is only one video playing, it is always treated as the
      // primary video, even if it doesn't cover the full window.
      primary = elem[i];
    } else if (rect.width == window.innerWidth ||
               rect.height == window.innerHeight) {
      primary = elem[i];
    }
  }
  return primary;
};

function extractState(video, disabledMediaCodecs) {
  let state = {};
  state.disabledMediaCodecs = disabledMediaCodecs;
  state.hasPrimaryVideo = false;
  if (video) {
    state.hasPrimaryVideo = true;
    state.paused = video.paused;
    state.currentTime = video.currentTime;
    state.duration = video.duration;
    state.defaultPlaybackRate = video.defaultPlaybackRate;
    state.playbackRate = video.playbackRate;
  }
  return JSON.stringify(state);
};

function getDisabledMediaCodecs() {
  return window.h5vcc.cVal.getValue("Media.DisabledMediaCodecs");
};

// NOTE: Place all public members and methods of |_mediaConsoleContext|
// below. They form closures with the above "private" members and methods
// and hence can use them directly, without referencing |this|.

ctx.getPlayerState = function() {
  const video = getPrimaryVideo();
  const disabledMediaCodecs = getDisabledMediaCodecs();
  return extractState(video, disabledMediaCodecs);
};

ctx.togglePlayPause = function() {
  let video = getPrimaryVideo();
  if (video) {
    if (video.paused) {
      video.play();
    } else {
      video.pause();
    }
  }
  return extractState(video, getDisabledMediaCodecs());
};

ctx.increasePlaybackRate = function() {
  let video = getPrimaryVideo();
  if (video) {
    let i = kPlaybackRates.indexOf(video.playbackRate);
    i = Math.min(i + 1, kPlaybackRates.length - 1);
    video.playbackRate = kPlaybackRates[i];
  }
  return extractState(video, getDisabledMediaCodecs());
};

ctx.decreasePlaybackRate = function() {
  let video = getPrimaryVideo();
  if (video) {
    let i = kPlaybackRates.indexOf(video.playbackRate);
    i = Math.max(i - 1, 0);
    video.playbackRate = kPlaybackRates[i];
  }
  return extractState(video, getDisabledMediaCodecs());
};

ctx.getSupportedCodecs = function() {
  // By querying all the possible mime and codec pairs, we can determine
  // which codecs are valid to control and toggle.  We use arbitrarily-
  // chosen codec subformats to determine if the entire family is
  // supported.
  const kVideoCodecs = [
      'av01.0.04M.10.0.110.09.16.09.0',
      'avc1.640028',
      'hvc1.1.2.L93.B0',
      'vp09.02.10.10.01.09.16.09.01',
      'vp8',
      'vp9',
  ];
  const kVideoMIMEs = [
      'video/mpeg',
      'video/mp4',
      'video/ogg',
      'video/webm',
  ];
  const kAudioCodecs = [
      'ac-3',
      'ec-3',
      'mp4a.40.2',
      'opus',
      'vorbis',
  ];
  const kAudioMIMEs = [
      'audio/flac',
      'audio/mpeg',
      'audio/mp3',
      'audio/mp4',
      'audio/ogg',
      'audio/wav',
      'audio/webm',
      'audio/x-m4a',
  ];

  let results = [];
  kVideoMIMEs.forEach(mime => {
    kVideoCodecs.forEach(codec => {
      let family = codec.split('.')[0];
      let mimeCodec = mime + '; codecs ="' + codec + '"';
      results.push([family, MediaSource.isTypeSupported(mimeCodec)]);
    })
  });
  kAudioMIMEs.forEach(mime => {
    kAudioCodecs.forEach(codec => {
      let family = codec.split('.')[0];
      let mimeCodec = mime + '; codecs ="' + codec + '"';
      results.push([family, MediaSource.isTypeSupported(mimeCodec)]);
    })
  });
  return JSON.stringify(results);
}

})()
