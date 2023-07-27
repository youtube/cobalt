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


// The MessageLog class manages the storage and display of all log messages in
// the debug console.
//
// Two primary components are used:
// 1. A buffer containing all the messages, MessageBuffer. This typically
//    contains many more messages than can be displayed on a single screen.
// 2. An HTML container filled with elements for each message. This container
//    is passed in to the MessageLog constructor.
//
// The MessageLog class creates a MessageBuffer object to contain the
// messages, and populates the HTML container to display the paginated
// messages. Each HTML element is not created until the corresponding message
// needs to be displayed, so if the debug console is initially invisible,
// no HTML nodes will be created until it becomes visible.

// Constructor for a single entry in the message buffer.
function BufferEntry(severity, message) {
  this.severity = severity;
  this.message = message;
  this.element = null;
}

// Constructor for the actual buffer used to store messages.
// This is implemented as a simple circular buffer of BufferEntry objects.
function MessageBuffer() {
  // The maximum number of entries that can be stored. Not all of these will
  // be simultaneously displayed.
  this.MAX_BUFFER_SIZE = 500;
  // Array of BufferEntry objects, used as a circular buffer.
  this.entries = [];
  // Current buffer insert position.
  this.insertPos = 0;
  // Current number of items in the buffer.
  this.size = 0;
}

// Add a new item to the message buffer at the current insert point.
MessageBuffer.prototype.insert = function(bufferEntry) {
  this.entries[this.insertPos] = bufferEntry;
  if (this.size < this.MAX_BUFFER_SIZE) {
    this.size += 1;
  }
  // Increment insert position with wraparound.
  this.insertPos = (this.insertPos + 1) % this.MAX_BUFFER_SIZE;
}

// Get an item from the message buffer at a logical index, where 0 corresponds
// to the last inserted item, and index N is the Nth last item to be inserted.
MessageBuffer.prototype.get = function(index) {
  if (index >= this.size) {
    return null;
  }
  // The index parameter is relative to the current insert position.
  var offsetIndex = (this.insertPos - 1 - index);
  // Buffer wraparound
  while (offsetIndex < 0) {
    offsetIndex += this.MAX_BUFFER_SIZE;
  }
  offsetIndex = offsetIndex % this.MAX_BUFFER_SIZE;
  return this.entries[offsetIndex];
}

// Constructor for the message log object itself.
function MessageLog(messageContainer) {
  // Number of items to display on a single page.
  this.PAGE_SIZE = 50;
  // Number of items to scroll when the user pages up or down.
  this.SCROLL_SIZE = 20;
  // Number of lines to display when paging up to beginning of buffer.
  this.DISPLAY_AT_HEAD = 30;
  // The parent HTML element that holds the message elements.
  this.messageContainer = messageContainer;
  // The buffer that holds the message entries.
  this.buffer = new MessageBuffer();
  // Index of the item at the bottom of the currently displayed page.
  this.displayPos = 0;
  // Whether the message log is currently visible.
  this.visible = false;
}

// Log levels defined by the 'level' property of LogEntry
// https://chromedevtools.github.io/devtools-protocol/1-3/Log#type-LogEntry
MessageLog.VERBOSE = "verbose";
MessageLog.INFO = "info";
MessageLog.WARNING = "warning";
MessageLog.ERROR = "error";
// Custom level used internally by the console.
MessageLog.INTERACTIVE = "interactive";
// Prefix on severity for messages from the JS console.
MessageLog.CONSOLE = '*';

MessageLog.prototype.setVisible = function(visible) {
  var wasVisible = this.visible;
  this.visible = visible;
  if (this.visible) {
    if (!wasVisible) {
      // Newly visible, display the messages on the current page.
      this.displayMessages();
    }
  } else {
    // Invisible. In theory, we might want to clear any HTML elements here,
    // but in practice, because of deferred GC, it doesn't gain us anything,
    // and in fact seems to result in more HTML nodes most of the time.
  }
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
  elem.className = 'message';
  var text = document.createTextNode(message);

  if (severity.startsWith(MessageLog.CONSOLE)) {
    severity = severity.substr(MessageLog.CONSOLE.length);
    elem.classList.add('console');
  }

  elem.classList.add(severity);

  elem.appendChild(text);

  return elem;
}

// Adds a new message to the log. Stores it in the buffer, and updates the
// display.
MessageLog.prototype.addMessage = function(severity, message) {
  // Split into lines and add one entry per line.
  // This makes it much simpler to manage layout with pagination.
  var lines = message.split('\n');
  var nLines = lines.length;
  // Ignore an empty line at the end - it just indicates a trailing newline.
  if (nLines >= 2 && lines[nLines - 1] == '') {
    nLines -= 1;
  }
  for (var l = 0; l < nLines; l++) {
    // Create the buffer entry and add it to the buffer.
    // Don't create the HTML element yet - defer until display.
    var newEntry = new BufferEntry(severity, lines[l]);
    this.buffer.insert(newEntry);
  }
  this.displayPos = 0;
  this.displayMessages();
}

// Completely repopulates the current message container, taking into account
// the current pagination settings.
MessageLog.prototype.displayMessages = function() {
  // Don't do anything if not currently visible.
  if (!this.visible) {
    return;
  }
  // Remove all currently displayed message elements and repopulate.
  while (this.messageContainer.firstChild) {
    this.messageContainer.removeChild(this.messageContainer.firstChild);
  }
  for (var n = this.PAGE_SIZE; n >= 0; n--) {
    var idx = n + this.displayPos;
    var bufferEntry = this.buffer.get(idx);
    if (bufferEntry) {
      // Create the HTML element if necessary.
      if (!bufferEntry.element) {
        bufferEntry.element = this.createMessageElement(bufferEntry.severity,
                                                        bufferEntry.message);
      }
      this.messageContainer.appendChild(bufferEntry.element);
    }
  }
}

MessageLog.prototype.scrollUp = function(size) {
  this.displayPos += size;
  var max = this.buffer.size - this.DISPLAY_AT_HEAD;
  if (this.displayPos > max) {
    this.displayPos = max;
  }
  this.displayMessages();
}

MessageLog.prototype.scrollDown = function(size) {
  this.displayPos -= size;
  if (this.displayPos < 0) {
    this.displayPos = 0;
  }
  this.displayMessages();
}

MessageLog.prototype.pageUp = function() {
  this.scrollUp(this.SCROLL_SIZE);
}

MessageLog.prototype.pageDown = function() {
  this.scrollDown(this.SCROLL_SIZE);
}

MessageLog.prototype.toHead = function() {
  this.displayPos = this.buffer.size - this.DISPLAY_AT_HEAD;
  this.displayMessages();
}

MessageLog.prototype.toTail = function() {
  this.displayPos = 0;
  this.displayMessages();
}
