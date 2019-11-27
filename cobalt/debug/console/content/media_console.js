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

const kNewline = '\r\n';

const kMediaConsoleFrame = 'mediaConsoleFrame';

function MediaConsole(debuggerClient) {
  let mediaConsoleNode = document.getElementById('mediaConsole');

  this.debuggerClient = debuggerClient;
  this.lastUpdateTime = window.performance.now();
  this.updatePeriod = 1000;

  // Registry of all the hotkeys and their function handlers.
  this.hotkeyRegistry = new Map([
      ['p', {handler: this.onPlayPause.bind(this), help: '(p)ause/(p)lay'}],
      [']', {handler: this.onIncreasePlaybackRate.bind(this),
        help: '(])Increase Playback Rate'}],
      ['[', {handler: this.onDecreasePlaybackRate.bind(this),
        help: '([)Decrease Playback Rate'}],
  ]);

  // Generate the help text that will be displayed at the top of the console
  // output.
  let hotkeysHelp = 'Hotkeys:' + kNewline;
  const generateHelpText = function(hotkeyInfo, hotkey, map) {
    hotkeysHelp += hotkeyInfo.help + ', ';
  };
  this.hotkeyRegistry.forEach(generateHelpText);
  hotkeysHelp = hotkeysHelp.substring(0, hotkeysHelp.length-2);
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;
  mediaConsoleNode.appendChild(document.createTextNode(hotkeysHelp));

  // Dynamically added div will be changed as we get state information.
  let playerStateElem = document.createElement('div');
  this.playerStateText = document.createTextNode('');
  playerStateElem.appendChild(this.playerStateText);
  mediaConsoleNode.appendChild(playerStateElem);

  this.printToMediaConsoleCallback = this.printToMediaConsole.bind(this);
}

MediaConsole.prototype.initialize = function() {
  this.debuggerClient.attach();
  // TODO: Move this code into a js file and have the script dynamically load it
  // from the content folder, rather than sending it as a raw string.
  // TODO: Fix |getPrimaryVideo| implementation to actually select the primary
  // player.
  this.debuggerClient.evaluate(`
    window._mediaConsoleContext ||
    (function() {
      let ctx = window._mediaConsoleContext = {};

      // NOTE: Place all "private" members and methods of |_mediaConsoleContext|
      // below. Private functions are not attached to the |_mediaConsoleContext|
      // directly. Rather they are referenced within the public functions, which
      // prevent it from being garbage collected.

      const kPlaybackRates = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0];

      function getPrimaryVideo() {
        const elem = document.querySelectorAll('video');
        if (elem && elem.length > 0) {
          let primary = null;
          for (let i = 0; i < elem.length; i++) {
            const rect = elem[i].getBoundingClientRect();
            if (rect.width == window.innerWidth &&
                rect.height == window.innerHeight) {
              if (primary != null) {
                console.warn('Two video elements found with the same\
                    dimensions as the main window.');
              }
              primary = elem[i];
            }
          }
          if (primary == null) {
            console.warn('Video elements found in page, but none were \
                fullscreen, and hence there is no primary video.');
          }
          return primary;
        }
        return null;
      };

      function extractStateFromVideo(video) {
        if (video) {
          let state = {};
          state.paused = video.paused;
          state.currentTime = video.currentTime;
          state.duration = video.duration;
          state.defaultPlaybackRate = video.defaultPlaybackRate;
          state.playbackRate = video.playbackRate;
          return JSON.stringify(state);
        }
        return null;
      };

      // NOTE: Place all public members and methods of |_mediaConsoleContext|
      // below. They form closures with the above "private" members and methods
      // and hence can use them directly, without referencing |this|.

      ctx.getPlayerState = function() {
        const video = getPrimaryVideo();
        return extractStateFromVideo(video);
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
        return extractStateFromVideo(video);
      };

      ctx.increasePlaybackRate = function() {
        let video = getPrimaryVideo();
        if (video) {
          let i = kPlaybackRates.indexOf(video.playbackRate);
          i = Math.min(i + 1, kPlaybackRates.length - 1);
          video.playbackRate = kPlaybackRates[i];
        }
        return extractStateFromVideo(video);
      };

      ctx.decreasePlaybackRate = function() {
        let video = getPrimaryVideo();
        if (video) {
          let i = kPlaybackRates.indexOf(video.playbackRate);
          i = Math.max(i - 1, 0);
          video.playbackRate = kPlaybackRates[i];
        }
        return extractStateFromVideo(video);
      };

    }());
  `);
}

MediaConsole.prototype.parseStateFromResponse = function(response) {
  // TODO: Handle the situation where the response contains exceptionDetails.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-evaluate
  return JSON.parse(response.result.value);
}

MediaConsole.prototype.printToMediaConsole = function(response) {
  const state = this.parseStateFromResponse(response);
  this.playerStateText.textContent =
      `Primary Video:
      Paused: ${state.paused} ${kNewline} \
      Current Time: ${state.currentTime} ${kNewline} \
      Duration: ${state.duration} ${kNewline} \
      Playback Rate: ${state.playbackRate} \
      (default: ${state.defaultPlaybackRate})`;
}

MediaConsole.prototype.update = function() {
  const t = window.performance.now();
  if (t > this.lastUpdateTime + this.updatePeriod) {
    this.lastUpdateTime = t;
    this.debuggerClient.evaluate('_mediaConsoleContext.getPlayerState()',
        this.printToMediaConsoleCallback);
  }
}

MediaConsole.prototype.setVisible = function(doShow) {
  let elem = document.getElementById(kMediaConsoleFrame);
  if (elem) {
    const display = doShow ? 'block' : 'none';
    if (elem.style.display != display) {
      elem.style.display = display;
      this.initialize(); // Will do nothing if already initialized.
    }
  }
}

MediaConsole.prototype.onWheel = function(event) {}

MediaConsole.prototype.onKeyup = function(event) {}

MediaConsole.prototype.onKeypress = function(event) {}

MediaConsole.prototype.onKeydown = function(event) {
  if (this.hotkeyRegistry.has(event.key)) {
    const item = this.hotkeyRegistry.get(event.key);
    if (item && item.handler) {
      item.handler();
      return;
    }
    console.log(`Failed to handle hotkey ${event.key}.`);
  }
}

MediaConsole.prototype.onPlayPause = function() {
  this.debuggerClient.evaluate('_mediaConsoleContext.togglePlayPause()',
      this.printToMediaConsoleCallback);
}

MediaConsole.prototype.onIncreasePlaybackRate = function() {
  this.debuggerClient.evaluate('_mediaConsoleContext.increasePlaybackRate()',
      this.printToMediaConsoleCallback);
}

MediaConsole.prototype.onDecreasePlaybackRate = function() {
  this.debuggerClient.evaluate('_mediaConsoleContext.decreasePlaybackRate()',
      this.printToMediaConsoleCallback);
}
