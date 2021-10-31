// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Global for other modules to use.
function printToMessageLog(severity, message) {
  if (window.debugConsoleInstance) {
    window.debugConsoleInstance.printToMessageLog(severity, message);
  }
}

function DebugConsole(debuggerClient) {
  window.debugConsoleInstance = this;

  this.debuggerClient = debuggerClient;
  // Text the user has typed since last hitting Enter
  this.inputText = '';

  this.commandInput = new CommandInput(document.getElementById('in'));
  this.messageLog = new MessageLog(document.getElementById('messageContainer'));

  this.consoleValues = new ConsoleValues();
  let loadResult = this.consoleValues.loadActiveSet();
  this.printToMessageLog(MessageLog.INTERACTIVE, loadResult);

  this.debugCommands = new DebugCommands(this);
}

DebugConsole.prototype.printToMessageLog = function(severity, message) {
  this.messageLog.addMessage(severity, message);
}

DebugConsole.prototype.printToHud = function(message) {
  let elem = document.getElementById('hud');
  elem.textContent = message;
}

DebugConsole.prototype.updateHud = function() {
  let mode = window.debugHub.getDebugConsoleMode();
  this.consoleValues.update();
  let cvalString = this.consoleValues.toString();
  this.printToHud(cvalString);
}

DebugConsole.prototype.setVisible = function(visible) {
  this.messageLog.setVisible(visible);
}

DebugConsole.prototype.update = function() {
  this.commandInput.animateBlink();
  this.updateHud();
}

// Executes a command from the history buffer.
// Index should be an integer (positive is an absolute index, negative is a
// number of commands back from the current) or '!' to execute the last command.
DebugConsole.prototype.executeCommandFromHistory = function(idx) {
  if (idx == '!') {
    idx = -1;
  }
  idx = parseInt(idx);
  this.commandInput.setCurrentCommandFromHistory(idx);
  this.executeCurrentCommand();
}

// Special commands that are executed immediately, not as JavaScript,
// e.g. !N to execute the Nth command in the history buffer.
// Returns true if the command is processed here, false otherwise.
DebugConsole.prototype.executeImmediate = function(command) {
  if (command[0] == '!') {
    this.executeCommandFromHistory(command.substring(1));
    return true;
  } else if (command.trim() == 'help') {
    // Treat 'help' as a special case for users not expecting JS execution.
    this.debugCommands.help();
    this.commandInput.clearCurrentCommand();
    return true;
  }
  return false;
}

// Execute a command as JavaScript in the main web module.
// Use the debugger evaluate command, which gives us Command Line API access
// and rich results with object preview.
DebugConsole.prototype.executeMain = function(command) {
  let callback = this.printToLogCallback.bind(this);
  this.debuggerClient.evaluate(command, callback);
}

// Executes a command entered by the user.
// Commands are processed in the following order:
// 1. Check for an 'immediate' command, e.g. history - !n.
// 2. Execute debug JS commands in this web module.
// 3. If no matching command is found, pass to the Cobalt DebugHub.
//    DebugHub will execute any commands recognized on the C++ side,
//    or pass to the main web module to be executed as JavaScript.
DebugConsole.prototype.executeCommand = function(command) {
  if (this.executeImmediate(command)) {
    this.printToMessageLog(MessageLog.INTERACTIVE, '');
    return;
  }
  this.commandInput.storeAndClearCurrentCommand();
  if (this.debugCommands.executeCommand(command)) {
    this.printToMessageLog(MessageLog.INTERACTIVE, '');
    return;
  }
  this.executeMain(command);
}

// Executes the current command in the CommandInput object.
// Typically called when the user hits Enter.
DebugConsole.prototype.executeCurrentCommand = function() {
  let command = this.commandInput.getCurrentCommand();
  this.printToMessageLog(MessageLog.INTERACTIVE, '> ' + command);
  this.executeCommand(command);
}

DebugConsole.prototype.onWheel = function(event) {
  if (event.deltaY > 0) {
    this.messageLog.scrollDown(event.deltaY);
  } else if (event.deltaY < 0) {
    this.messageLog.scrollUp(-event.deltaY);
  }
}

DebugConsole.prototype.onKeydown = function(event) {
  let key = event.key;

  if (key == 'ArrowLeft') {
    this.commandInput.moveCursor(-1);
  } else if (key == 'ArrowRight') {
    this.commandInput.moveCursor(1);
  } else if (key == 'ArrowUp') {
    this.commandInput.back();
  } else  if (key == 'ArrowDown') {
    this.commandInput.forward();
  } else if (key == 'Backspace') {
    this.commandInput.deleteCharBehindCursor();
  } else if (key == 'Enter') {
    this.executeCurrentCommand();
  } else if (key == 'PageUp') {
    this.messageLog.pageUp();
  } else if (key == 'PageDown') {
    this.messageLog.pageDown();
  } else if (key == 'Delete') {
    this.commandInput.deleteCharAtCursor();
  } else if (key == 'Home') {
    if (event.ctrlKey) {
      this.messageLog.toHead();
    } else {
      this.commandInput.moveCursor(-1000);
    }
  } else if (key == 'End') {
    if (event.ctrlKey) {
      this.messageLog.toTail();
    } else {
      this.commandInput.moveCursor(1000);
    }
  }
}

DebugConsole.prototype.onKeyup = function(event) {}

DebugConsole.prototype.onKeypress = function(event) {
  event.preventDefault();
  event.stopPropagation();
  let c = event.charCode;
  // If we have a printable character, insert it; otherwise ignore.
  if (c >= 0x20 && c <= 0x7e) {
    this.commandInput.insertStringBehindCursor(String.fromCharCode(c));
  }
}

DebugConsole.prototype.onInput = function(event) {
  console.log('In DebugConsole onInput, event.data ' + event.data);
  if (event.data) {
    event.preventDefault();
    event.stopPropagation();
    this.commandInput.insertStringBehindCursor(event.data);
  }
}

DebugConsole.prototype.printToLogCallback = function(result) {
  if (result.wasThrown) {
    this.printToMessageLog(MessageLog.ERROR,
                      'Uncaught ' + result.result.description);
  } else if (result.result.preview) {
    this.printToMessageLog(MessageLog.INFO, result.result.preview.description);
    if (result.result.preview.properties) {
      for (let i = 0; i < result.result.preview.properties.length; ++i) {
        let property = result.result.preview.properties[i];
        this.printToMessageLog(MessageLog.INFO,
                          '  ' + property.name + ': ' + property.value);
      }
    }
    if (result.result.preview.overflow) {
      this.printToMessageLog(MessageLog.INFO, '  ...');
    }
  } else if (result.result.description) {
    this.printToMessageLog(MessageLog.INFO, result.result.description);
  } else if (result.result.value) {
    this.printToMessageLog(MessageLog.INFO, result.result.value.toString());
  }
  this.printToMessageLog(MessageLog.INFO, '');
}
