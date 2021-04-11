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
  let js = debugHub.readDebugContentText('media_console_context.js');
  this.debuggerClient.evaluate(js);
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

MediaConsole.prototype.update = function(forceUpdate) {
  const t = window.performance.now();
  if (forceUpdate || t > this.lastUpdateTime + this.updatePeriod) {
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
  if (codecs.length == toggled.length) { toggled.push(codecToToggle); }
  this.setDisabledMediaCodecs(toggled.join(';'));
  // Force an immediate update of the console to reflect the above change.
  this.update(true);
}
