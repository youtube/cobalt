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

// The MessageLog class manages the storage and display of all log messages in
// the remote console.
// All messages are stored and displayed as HTML elements inside a scrolling
// frame.

// Constructor for the message log object itself.
function MessageLog(messageContainer) {
  this.INTERACTIVE = 0;
  this.RESPONSE = 1;
  this.WARNING = 2;
  this.ERROR = 3;
  this.NOTIFICATION = 4;
  this.LOCAL = 5;
  this.messageContainer = messageContainer;
  this.visible = false;
}

// Creates the HTML element for a single message.
MessageLog.prototype.createMessageElement = function(severity, message) {
  // An empty message will result in an empty box occupying no space.
  // Insert a space into empty messages to preserve line formatting.
  if (message.length <= 0) {
    message = ' ';
  };

  // Create the new text element with the message.
  var elem = document.createElement('div');
  elem.style.whiteSpace = 'pre';
  var text = document.createTextNode(message);

  if (severity == this.INTERACTIVE) {
    elem.style.color = '#000000';
  } else if (severity == this.RESPONSE) {
    elem.style.color = '#00A000';
  } else if (severity == this.WARNING) {
    elem.style.color = '#A08000';
  } else if (severity == this.ERROR) {
    elem.style.color = '#FF0000';
  } else if (severity == this.NOTIFICATION) {
    elem.style.color = '#4080FF';
  } else {
    elem.style.color = '#707070';
  }
  elem.appendChild(text);

  return elem;
}

MessageLog.prototype.addMessage = function(severity, message) {
  var elem = this.createMessageElement(severity, message);
  this.messageContainer.appendChild(elem);
  this.messageContainer.scrollTop = this.messageContainer.scrollHeight;
}

