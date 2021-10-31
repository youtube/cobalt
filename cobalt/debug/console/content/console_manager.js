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

var consoleManager = null;

function start() {
  window.consoleManager = new ConsoleManager();
}

window.addEventListener('load', start);

function ConsoleManager() {
  // Handles communication with the debugger.
  this.debuggerClient = new DebuggerClient();
  // Number of animation frame samples since the last update.
  this.animationFrameSamples = 0;
  // A list of all the possible interactive consoles.
  // Each item contains the mode and the reference to the actual console.
  this.consoleRegistry = null;

  this.initializeConsoles();

  document.addEventListener('keydown', this.handleKeydown.bind(this));
  document.addEventListener('keyup', this.handleKeyup.bind(this));
  document.addEventListener('keypress', this.handleKeypress.bind(this));
  document.addEventListener('wheel', this.handleWheel.bind(this));
  document.addEventListener('input', this.handleInput.bind(this));
  if (typeof window.onScreenKeyboard != 'undefined'
      && window.onScreenKeyboard) {
    window.onScreenKeyboard.onInput = this.handleInput.bind(this);
  }
  window.requestAnimationFrame(this.animate.bind(this));
}

ConsoleManager.prototype.initializeConsoles = function() {
  this.consoleRegistry = [
    {
      console: new DebugConsole(this.debuggerClient),
      mode: 'debug',
      bodyClass: 'debugConsole hud',
    },
    {
      console: new MediaConsole(this.debuggerClient),
      mode: 'media',
      bodyClass: 'mediaConsole',
    },
  ];

  this.hudConsole = this.consoleRegistry[0].console;

  this.consoleRegistry.forEach( entry => {
    let ensureConsolesAreValid = function(method) {
      if(typeof entry.console[method] != "function") {
        console.warn(`Console "${entry.mode}" ${method}() is not implemented. \
            Providing default empty implementation.`);
        // Provide a default not-implemented warning message.
        let consoleName = entry.mode;
        let notImplementedMessage = function() {
          console.log(
              `Console "${consoleName}" ${method}() is not implemented.`);
        };
        entry.console[method] = notImplementedMessage;
      }
    };
    ensureConsolesAreValid("update");
    ensureConsolesAreValid("setVisible");
    ensureConsolesAreValid("onKeydown");
    ensureConsolesAreValid("onKeyup");
    ensureConsolesAreValid("onKeypress");
    ensureConsolesAreValid("onInput");
    ensureConsolesAreValid("onWheel");
  });
}

ConsoleManager.prototype.update = function() {
  let mode = window.debugHub.getDebugConsoleMode();

  if (mode !== 'off') {
    this.debuggerClient.attach();
  }

  let activeConsole = this.getActiveConsole();
  let bodyClass = '';
  if (mode == 'hud') {
    bodyClass = 'hud';
    // The HUD is owned by the debug console, but since it has its own mode
    // dedicated to it, it needs to be specifically updated when it is visible.
    // TODO: Factor out hudConsole into its own console.
    this.hudConsole.updateHud();
  } else if (mode != 'off' && mode != 'hud') {
    bodyClass = activeConsole.bodyClass;
  }
  document.body.className = bodyClass;

  this.consoleRegistry.forEach((entry) => {
    entry.console.setVisible(entry == activeConsole);
  });

  if (mode !== 'off') {
    if (activeConsole) { activeConsole.console.update(); }
  }
}

// Animation callback: updates state and animated nodes.
ConsoleManager.prototype.animate = function(time) {
  const subsample = 8;
  this.animationFrameSamples = (this.animationFrameSamples + 1) % subsample;
  if (this.animationFrameSamples == 0) {
    this.update();
  }
  window.requestAnimationFrame(this.animate.bind(this));
}

ConsoleManager.prototype.getActiveConsole = function() {
  let mode = window.debugHub.getDebugConsoleMode();
  return this.consoleRegistry.find( entry => entry.mode === mode );
}

ConsoleManager.prototype.handleKeydown =  function(event) {
  // Map of 'Unidentified' additional Cobalt keyCodes to equivalent keys.
  const unidentifiedCobaltKeyMap = {
    // kSbKeyGamepad1
    0x8000: 'Enter',
    // kSbKeyGamepad2
    0x8001: 'Esc',
    // kSbKeyGamepad3
    0x8002: 'Home',
    // kSbKeyGamepad5
    0x8008: 'Enter',
    // kSbKeyGamepad6
    0x8009: 'Enter',
    // kSbKeyGamepadDPadUp
    0x800C: 'ArrowUp',
    // kSbKeyGamepadDPadDown
    0x800D: 'ArrowDown',
    // kSbKeyGamepadDPadLeft
    0x800E: 'ArrowLeft',
    // kSbKeyGamepadDPadRight
    0x800F: 'ArrowRight',
    // kSbKeyGamepadLeftStickUp
    0x8011: 'ArrowUp',
    // kSbKeyGamepadLeftStickDown
    0x8012: 'ArrowDown',
    // kSbKeyGamepadLeftStickLeft
    0x8013: 'ArrowLeft',
    // kSbKeyGamepadLeftStickRight
    0x8014: 'ArrowRight',
    // kSbKeyGamepadRightStickUp
    0x8015: 'ArrowUp',
    // kSbKeyGamepadRightStickDown
    0x8016: 'ArrowDown',
    // kSbKeyGamepadRightStickLeft
    0x8017: 'ArrowLeft',
    // kSbKeyGamepadRightStickRight
    0x8018: 'ArrowRight'
  };

  let key = event.key;
  if (key == 'Unidentified') {
    key = unidentifiedCobaltKeyMap[event.keyCode] || 'Unidentified';
  }

  let active = this.getActiveConsole();
  if (active) { active.console.onKeydown(event); }
}

ConsoleManager.prototype.handleKeyup = function(event) {
  let active = this.getActiveConsole();
  if (active) { active.console.onKeyup(event); }
}

ConsoleManager.prototype.handleKeypress = function(event) {
  let active = this.getActiveConsole();
  if (active) { active.console.onKeypress(event); }
}

ConsoleManager.prototype.handleInput = function(event) {
  let active = this.getActiveConsole();
  if (active) { active.console.onInput(event); }
}

ConsoleManager.prototype.handleWheel = function(event) {
  let active = this.getActiveConsole();
  if (active) { active.console.onWheel(event); }
}
