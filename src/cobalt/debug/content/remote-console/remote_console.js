/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Text the user has typed since last hitting Enter
var inputText = '';
// The DOM node used as a container for message nodes.
var messageLog = null;
// Handles command input, editing, history traversal, etc.
var commandInput = null;
// Websocket connection to the debug server host.
var connection = null;

function createMessageLog() {
  var messageContainer = document.getElementById('messageContainer');
  messageLog = new MessageLog(messageContainer);
}

function createCommandInput() {
  var inputElem = document.getElementById('in');
  this.commandInput = new CommandInput(inputElem);
}

function showBlockElem(elem, doShow) {
  if (elem) {
    var display = doShow ? 'block' : 'none';
    elem.style.display = display;
  }
}

function showConsole(doShow) {
  showBlockElem(document.getElementById('consoleFrame'), doShow);
}

function printToMessageLog(severity, message) {
  messageLog.addMessage(severity, message);
}

// Animation callback: updates state and animated nodes.
function animate(time) {
  commandInput.animateBlink();
  window.requestAnimationFrame(animate);
}

// Executes a command from the history buffer.
// Index should be an integer (positive is an absolute index, negative is a
// number of commands back from the current) or '!' to execute the last command.
function executeCommandFromHistory(idx) {
  if (idx == '!') {
    idx = -1;
  }
  idx = parseInt(idx);
  commandInput.setCurrentCommandFromHistory(idx);
  executeCurrentCommand();
}

// Special commands that are executed immediately, not as JavaScript,
// e.g. !N to execute the Nth command in the history buffer.
// Returns true if the command is processed here, false otherwise.
function executeImmediate(command) {
  if (command[0] == '!') {
    executeCommandFromHistory(command.substring(1));
    return true;
  } else if (command.trim() == 'help') {
    // Treat 'help' as a special case for users not expecting JS execution.
    help();
    commandInput.clearCurrentCommand();
    return true;
  }
  return false;
}

// Executes a command entered by the user.
function executeCommand(command) {
  if (executeImmediate(command)) {
    return;
  }
  commandInput.storeAndClearCurrentCommand();
  try {
    var result = eval(command);
    if (result != undefined && result != null) {
      printToMessageLog(messageLog.LOCAL, result.toString());
    }
  } catch(error) {
    printToMessageLog(messageLog.ERROR, error.toString());
  }
}

// Executes the current command in the CommandInput object.
// Typically called when the user hits Enter.
function executeCurrentCommand() {
  var command = commandInput.getCurrentCommand();
  printToMessageLog(messageLog.INTERACTIVE, '\n> ' + command);
  executeCommand(command);
}

function onKeydown(event) {
  event.stopPropagation();
  var key = event.keyCode;
  if (key == 37) {
    event.preventDefault();
    commandInput.moveCursor(-1);
  } else if (key == 39) {
    event.preventDefault();
    commandInput.moveCursor(1);
  } else if (key == 38) {
    event.preventDefault();
    commandInput.back();
  } else  if (key == 40) {
    event.preventDefault();
    commandInput.forward();
  } else if (key == 8) {
    event.preventDefault();
    commandInput.deleteCharBehindCursor();
  } else if (key == 13) {
    event.preventDefault();
    executeCurrentCommand();
  } else if (key == 46) {
    event.preventDefault();
    commandInput.deleteCharAtCursor();
  } else if (key == 36) {
    event.preventDefault();
    commandInput.moveCursor(-1000);
  }
}

function onKeyup(event) {}

function onKeypress(event) {
  event.preventDefault();
  event.stopPropagation();
  var c = event.charCode;
  // If we have a printable character, insert it; otherwise ignore.
  if (c >= 0x20 && c <= 0x7e) {
    commandInput.insertCharBehindCursor(String.fromCharCode(c));
  }
}

function start() {
  createCommandInput();
  createMessageLog();
  showConsole(true);
  document.addEventListener('keypress', onKeypress);
  document.addEventListener('keydown', onKeydown);
  document.addEventListener('keyup', onKeyup);
  curr = window.performance.now();
  window.requestAnimationFrame(animate);
}

window.addEventListener('load', start);

