// Copyright 2015 Google Inc. All Rights Reserved.
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


function CommandInput(elem) {
  // The top-level HTML element this class uses for input interaction.
  this.inputElem = elem;
  // Buffer containing previously executed commands.
  this.commandHistory = [];
  // The text the user is currently editing in the input element.
  this.currCommand = '';
  // Index of the character at the cursor position (after the last
  // entry/deletion point).
  this.cursorPos = 0;
  // The period of the cursor blinking in milliseconds.
  this.blinkInterval = 600;
  // The time at which the cursor blink state was last updated (ms).
  this.lastBlink = 0;
  // The current blink state (on/off) of the cursor.
  this.blinkOn = false;
  // The index of the currently displayed command. If the current command has
  // not yet been stored in the history buffer, this index will be equal to the
  // length of the history buffer.
  this.commandIndex = 0;
  // Used to store the current command when traversing back through history.
  this.pendingCommand = '';
  this.updateText();
}

// The blinking cursor is always on the character immediately after the
// insertion/deletion position.
CommandInput.prototype.insertStringBehindCursor = function(c) {
  var cmd = this.currCommand;
  var pos = this.cursorPos;
  this.currCommand = cmd.substring(0, pos);
  this.currCommand += c;
  this.currCommand += cmd.substring(pos, cmd.length);
  this.cursorPos += c.length;
  this.updateText();
}

// The blinking cursor is always on the character immediately after the
// insertion/deletion position.
CommandInput.prototype.deleteCharBehindCursor = function() {
  var cmd = this.currCommand;
  var pos = this.cursorPos;
  if (cmd.length > 0 && pos > 0) {
    this.currCommand = cmd.substring(0, pos - 1);
    this.currCommand += cmd.substring(pos, cmd.length);
    this.cursorPos -= 1;
    this.updateText();
  }
}

CommandInput.prototype.deleteCharAtCursor = function() {
  var cmd = this.currCommand;
  var pos = this.cursorPos;
  if (cmd.length > 0 && pos < cmd.length) {
    this.currCommand = cmd.substring(0, pos);
    this.currCommand += cmd.substring(pos + 1, cmd.length);
    this.updateText();
  }
}

// The move parameter is an integer that specifies how many characters to the
// right to move the input cursor. A negative value will move the cursor that
// many characters to the right.
CommandInput.prototype.moveCursor = function(move) {
  this.cursorPos += move;
  if (this.cursorPos < 0) {
    this.cursorPos = 0;
  } else if (this.cursorPos > this.currCommand.length) {
    this.cursorPos = this.currCommand.length;
  }
  this.updateText();
}

// The currently displayed command. This can either be a new command, or a
// command from the history buffer that the user has navigated to.
CommandInput.prototype.getCurrentCommand = function() {
  return this.currCommand;
}

// Set the current command to any command from the history. This is used to
// support shell-style execution of any previously executed command.
// The idx parameter should be an integer: positive specifies an absolute
// index, negative a number of commands back from the current one.
CommandInput.prototype.setCurrentCommandFromHistory = function(idx) {
  if (idx < 0) {
    idx = this.commandHistory.length + idx;
  }
  if (idx >= 0 && idx < this.commandHistory.length) {
    this.currCommand = this.commandHistory[idx];
  } else {
    this.currCommand = '';
  }
}

// Set the current command to the previous command in the history buffer, if
// one exists. If the current command has not yet been executed, store it in
// the pending command member, in case the user decides to navigate forward to
// it again.
CommandInput.prototype.back = function() {
  if (this.commandIndex > 0) {
    if (this.commandIndex == this.commandHistory.length) {
      this.pendingCommand = this.currCommand;
    }
    this.commandIndex -= 1;
    this.currCommand = this.commandHistory[this.commandIndex];
    this.cursorPos = this.currCommand.length;
    this.updateText();
  }
}

// Set the current command to the next command in the history buffer. If we
// have gone past the most recent command in the buffer, restore the pending
// command, if one exists.
CommandInput.prototype.forward = function() {
  if (this.commandIndex < this.commandHistory.length) {
    this.commandIndex += 1;
    if (this.commandIndex < this.commandHistory.length) {
      this.currCommand = this.commandHistory[this.commandIndex];
    } else {
      this.currCommand = this.pendingCommand;
    }
    this.cursorPos = this.currCommand.length;
    this.updateText();
  }
}

// Stores the current command in the history buffer to make it
// available for execution again using the back/forward methods.
// Only stores if the current command is different from the previous.
CommandInput.prototype.storeCurrentCommand = function() {
  var idx = this.commandHistory.length;
  if (this.commandHistory[idx - 1] != this.currCommand) {
    this.commandHistory[idx] = this.currCommand;
  }
}

// Clears the current command.
CommandInput.prototype.clearCurrentCommand = function() {
  this.commandIndex = this.commandHistory.length;
  this.currCommand = '';
  this.pendingCommand = '';
  this.cursorPos = 0;
  this.updateText();
}

// Stores and clears the current command. This will typically be called when
// the user presses Enter to execute the current command.
CommandInput.prototype.storeAndClearCurrentCommand = function() {
  this.storeCurrentCommand();
  this.clearCurrentCommand();
}

// Called on the animation timer to make the cursor blink with a period
// defined by this.blinkInterval.
CommandInput.prototype.animateBlink = function() {
  var t = window.performance.now();
  if (t > this.lastBlink + this.blinkInterval) {
    this.lastBlink = t;
    this.blinkOn = !this.blinkOn;
    this.updateCursor();
  }
}

CommandInput.prototype.escapeTagChars = function(text) {
  return text.replace(/&/g, '&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// Replaces tag characters with their escaped equivalents for safe HTML display.
// Adjusts the cursor position accordingly.
// After escaping the tag characters, the cursor span may be longer than
// one character, hence we calculate cursorStart/End.
CommandInput.prototype.escapeTagCharsAndAdjustCursor =
    function(rawCommand, cursorPos) {
  var beforeCursor = rawCommand.substring(0, cursorPos);
  var atCursor = rawCommand.substring(cursorPos, cursorPos + 1);
  var afterCursor = rawCommand.substring(cursorPos + 1, rawCommand.length);
  beforeCursor = this.escapeTagChars(beforeCursor);
  atCursor = this.escapeTagChars(atCursor);
  afterCursor = this.escapeTagChars(afterCursor);
  var result = {};
  result.commandAsHTML = beforeCursor + atCursor + afterCursor;
  result.cursorStart = beforeCursor.length;
  result.cursorEnd = beforeCursor.length + (atCursor.length || 1);
  return result;
}

// Called when the current command text changes.
CommandInput.prototype.updateText = function() {
  var modifiedCommand = this.escapeTagCharsAndAdjustCursor(this.currCommand,
                                                           this.cursorPos);
  var cursorStart = modifiedCommand.cursorStart;
  var cursorEnd = modifiedCommand.cursorEnd;
  var commandAsHTML = modifiedCommand.commandAsHTML;
  var html = '';

  if (cursorEnd <= commandAsHTML.length) {
    html = commandAsHTML.substring(0, cursorStart);
    html += '<span id="cursor">';
    html += commandAsHTML.substring(cursorStart, cursorEnd);
    html += '</span>';
    html += commandAsHTML.substring(cursorEnd, commandAsHTML.length);
  } else {
    html = commandAsHTML + '<span id="cursor"> </span>';
  }
  this.inputElem.innerHTML = html;
  this.blinkOn = true;
  this.updateCursor();
}

// Called when the text or the cursor position changes to maintain the blinking
// cursor in the correct position. This should only be called when necessary,
// as it creates HTML nodes.
CommandInput.prototype.updateCursor = function() {
  var cursor = document.querySelector('#cursor');
  if (this.blinkOn) {
    cursor.style.backgroundColor = '#FFFFFF';
    cursor.style.color = '#000000';
  } else {
    cursor.style.backgroundColor = '#000000';
    cursor.style.color = '#FFFFFF';
  }
}

CommandInput.prototype.getHistory = function() {
  return this.commandHistory;
}
