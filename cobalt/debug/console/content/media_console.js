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
  var mediaConsoleNode = document.getElementById('mediaConsole');

  this.debuggerClient = debuggerClient;
  this.lastUpdateTime = window.performance.now();
  this.updatePeriod = 1000;

  // Registry of all the hotkeys and their function handlers.
  this.hotkeyRegistry = new Map([
      ['p', {handler: this.onPlayPause.bind(this), help: '(P)ause/(P)lay'}],
  ]);

  // Generate the help text that will be displayed at the top of the console
  // output.
  var hotkeysHelp = 'Hotkeys:' + kNewline;
  var generateHelpText = function(hotkeyInfo, hotkey, map) {
    hotkeysHelp += hotkeyInfo.help + ', ';
  };
  this.hotkeyRegistry.forEach(generateHelpText);
  hotkeysHelp = hotkeysHelp.substring(0, hotkeysHelp.length-2);
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;
  hotkeysHelp += kNewline;
  mediaConsoleNode.appendChild(document.createTextNode(hotkeysHelp));

  // Dynamically added div will be changed as we get state information.
  var playerStateElem = document.createElement('div');
  this.playerStateText = document.createTextNode('');
  playerStateElem.appendChild(this.playerStateText);
  mediaConsoleNode.appendChild(playerStateElem);
}

MediaConsole.prototype.parseStateFromResponse = function(response) {
  // TODO: Handle the situation where the response contains exceptionDetails.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-evaluate
  return JSON.parse(response.result.value);
}

MediaConsole.prototype.printToMediaConsole = function(response) {
  var state = this.parseStateFromResponse(response);
  this.playerStateText.textContent =
      'Paused: ' + state.paused + kNewline +
      'Current Time: ' + state.currentTime + kNewline +
      'Duration: ' + state.duration;
}

MediaConsole.prototype.getPlayerState = function(callback) {
  // TODO: Select the primary video by checking the bounding box dimensions.
  this.debuggerClient.evaluate(`
      (function() {
        var elem = document.querySelectorAll('video');
        var ret = {};
        if (elem && elem.length > 0) {
          primaryVideo = elem[0];
          ret = {
            paused: primaryVideo.paused,
            currentTime: primaryVideo.currentTime,
            duration: primaryVideo.duration
          };
        }
        return JSON.stringify(ret);
      })();
  `, callback);
}

MediaConsole.prototype.update = function() {
  var t = window.performance.now();
  if (t > this.lastUpdateTime + this.updatePeriod) {
    this.lastUpdateTime = t;
    var callback = this.printToMediaConsole.bind(this);
    this.getPlayerState(callback);
  }
}

MediaConsole.prototype.setVisible = function(doShow) {
  var elem = document.getElementById(kMediaConsoleFrame);
  if (elem) {
    var display = doShow ? 'block' : 'none';
    if (elem.style.display != display) {
      elem.style.display = display;
    }
  }
}

MediaConsole.prototype.onWheel = function(event) {}

MediaConsole.prototype.onKeyup = function(event) {}

MediaConsole.prototype.onKeypress = function(event) {}

MediaConsole.prototype.onKeydown = function(event) {
  if (this.hotkeyRegistry.has(event.key)) {
    var item = this.hotkeyRegistry.get(event.key);
    if (item && item.handler) {
      item.handler();
      return;
    }
    console.log(`Failed to handle hotkey ${event.key}.`);
  }
}

MediaConsole.prototype.onPlayPause = function() {
  var printToConsoleCallback = this.printToMediaConsole.bind(this);
  var updateState = this.getPlayerState.bind(this, printToConsoleCallback);

  // TODO: Select the primary video by checking the bounding box dimensions.
  this.debuggerClient.evaluate(`
    (function() {
      var elem = document.querySelectorAll('video');
      if (elem && elem.length > 0) {
        var primaryVideo = elem[0];
        if (primaryVideo.paused) {
          primaryVideo.play();
        } else {
          primaryVideo.pause();
        }
      }
    })();
  `, updateState);
  return true;
}
