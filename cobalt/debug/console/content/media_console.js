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

const videoCodecFamilies = ['av01', 'avc1', 'avc3', 'hev1', 'hvc1', 'vp09',
      'vp8', 'vp9'];
const audioCodecFamilies = ['ac-3', 'ec-3', 'mp4a', 'opus', 'vorbis'];

const kAltModifier = 'A-';
const kCtrlModifier = 'C-';

function MediaConsole(debuggerClient) {
  let mediaConsoleNode = document.getElementById('mediaConsole');

  this.debuggerClient = debuggerClient;
  this.getDisabledMediaCodecs = window.debugHub.cVal.getValue.bind(
      window.debugHub.cVal, 'Media.DisabledMediaCodecs');
  this.setDisabledMediaCodecs = window.debugHub.sendConsoleCommand.bind(
      window.debugHub, 'disable_media_codecs');
  this.lastUpdateTime = window.performance.now();
  this.updatePeriod = 1000;

  // Registry of all the hotkeys and their function handlers.
  // NOTE: This is not the full list, since more will be added once the codec
  // specific controls are registered.
  this.hotkeyRegistry = new Map([
      ['p', {handler: this.onPlayPause.bind(this), help: '(p)ause/(p)lay'}],
      [']', {handler: this.onIncreasePlaybackRate.bind(this),
        help: '(])Increase Playback Rate'}],
      ['[', {handler: this.onDecreasePlaybackRate.bind(this),
        help: '([)Decrease Playback Rate'}],
  ]);

  this.hotkeyHelpNode = document.createTextNode('');
  mediaConsoleNode.appendChild(this.hotkeyHelpNode);

  // Dynamically added div will be changed as we get state information.
  let playerStateElem = document.createElement('div');
  this.playerStateText = document.createTextNode('');
  playerStateElem.appendChild(this.playerStateText);
  mediaConsoleNode.appendChild(playerStateElem);

  this.printToMediaConsoleCallback = this.printToMediaConsole.bind(this);
  this.codecSupportMap = new Map();
  this.isInitialized = false;
}

MediaConsole.prototype.setVisible = function(visible) {
  if (visible) {
    this.initialize();
  }
}

MediaConsole.prototype.initialize = function() {
  if (this.initialized) { return; }

  this.debuggerClient.attach();
  this.initializeMediaConsoleContext();
  this.initializeSupportedCodecs();

  // Since |initializeSupportedCodecs| is resolved asynchronously, we need to
  // wait until |codecSupportMap| is fully populated before finishing the rest
  // of the initialization.
  if (this.codecSupportMap.size > 0) {
    this.registerCodecHotkeys();
    this.generateHotkeyHelpText();
    this.isInitialized = true;
  }
}

MediaConsole.prototype.initializeMediaConsoleContext = function() {
  // TODO: Move this code into a js file and have the script dynamically load it
  // from the content folder, rather than sending it as a raw string.

  this.debuggerClient.evaluate(`
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
          return primary;
        }
        return null;
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

    }());
  `);
}

MediaConsole.prototype.initializeSupportedCodecs = function() {
  if (this.codecSupportMap.size > 0) {
    return;
  }

  function handleSupportQueryResponse(response) {
    let results = JSON.parse(response.result.value);
    results.forEach(record => {
      let codecFamily = record[0];
      let isSupported = record[1] || !!this.codecSupportMap.get(codecFamily);
      this.codecSupportMap.set(codecFamily, isSupported);
    });
  };

  this.debuggerClient.evaluate(`_mediaConsoleContext.getSupportedCodecs()`,
      handleSupportQueryResponse.bind(this));
}

MediaConsole.prototype.registerCodecHotkeys = function() {
  // Codec control hotkeys are of the form: Modifier + Number.
  // Due to the large number of codecs, we split all the video codecs into Ctrl
  // and all the audio codecs into Alt.
  // "C-" prefix indicates a key with the ctrlKey modifier.
  // "A-" prefix indicates a key with the altKey modifier.
  // Additionally, we only register those hotkeys that we filter as supported.
  function registerCodecHotkey(modifier, codec, index) {
    if (!this.codecSupportMap.get(codec)) {
      return;
    }
    let modAndNum = `${modifier}${index + 1}`;
    let helpStr = `( ${modAndNum} ) ${codec}`;
    this.hotkeyRegistry.set(modAndNum, {
      handler: this.onToggleCodec.bind(this, codec),
      help: helpStr
    });
  };
  videoCodecFamilies.forEach(registerCodecHotkey.bind(this, kCtrlModifier));
  audioCodecFamilies.forEach(registerCodecHotkey.bind(this, kAltModifier));
}

MediaConsole.prototype.generateHotkeyHelpText = function() {
  // Generate the help text that will be displayed at the top of the console
  // output.
  let hotkeysHelp = `Hotkeys: ${kNewline}`;
  hotkeysHelp += `Ctrl+Num toggles video codecs ${kNewline}`;
  hotkeysHelp += `Alt+Num toggles audio codecs ${kNewline}`;
  const generateHelpText = function(hotkeyInfo, hotkey, map) {
    hotkeysHelp += hotkeyInfo.help + ', ';
  };
  this.hotkeyRegistry.forEach(generateHelpText);
  hotkeysHelp = hotkeysHelp.substring(0, hotkeysHelp.length-2);
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;

  this.hotkeyHelpNode.nodeValue = hotkeysHelp;
}

MediaConsole.prototype.parseStateFromResponse = function(response) {
  // TODO: Handle the situation where the response contains exceptionDetails.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-evaluate
  return JSON.parse(response.result.value);
}

MediaConsole.prototype.printToMediaConsole = function(response) {
  const state = this.parseStateFromResponse(response);
  let videoStatus = 'No primary video.';
  if(state.hasPrimaryVideo) { videoStatus = ''; }
  this.playerStateText.textContent =
      `Primary Video: ${videoStatus} ${kNewline} \
      Paused: ${state.paused} ${kNewline} \
      Current Time: ${state.currentTime} ${kNewline} \
      Duration: ${state.duration} ${kNewline} \
      Playback Rate: ${state.playbackRate} \
      (default: ${state.defaultPlaybackRate}) ${kNewline} \
      Disabled Media Codecs: ${state.disabledMediaCodecs}`;
}

MediaConsole.prototype.update = function() {
  const t = window.performance.now();
  if (t > this.lastUpdateTime + this.updatePeriod) {
    this.lastUpdateTime = t;
    this.debuggerClient.evaluate('_mediaConsoleContext.getPlayerState()',
        this.printToMediaConsoleCallback);
  }
}

MediaConsole.prototype.onWheel = function(event) {}

MediaConsole.prototype.onKeyup = function(event) {}

MediaConsole.prototype.onKeypress = function(event) {}

MediaConsole.prototype.onInput = function(event) {}

MediaConsole.prototype.onKeydown = function(event) {
  let matchedHotkeyEntry = null;
  this.hotkeyRegistry.forEach(function(hotkeyInfo, hotkey, map) {
    let eventKey = event.key;
    if(event.altKey) { eventKey = kAltModifier + eventKey; }
    if(event.ctrlKey) { eventKey = kCtrlModifier + eventKey; }
    if(eventKey == hotkey) {
      matchedHotkeyEntry = hotkeyInfo;
    }
  });
  if (matchedHotkeyEntry) {
    matchedHotkeyEntry.handler();
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

MediaConsole.prototype.onToggleCodec = function(codecToToggle) {
  let codecs = this.getDisabledMediaCodecs().split(";");
  codecs = codecs.filter(s => s.length > 0);
  toggled = codecs.filter(c => c != codecToToggle);
  if(codecs.length == toggled.length) { toggled.push(codecToToggle); }
  this.setDisabledMediaCodecs(toggled.join(';'));
}
