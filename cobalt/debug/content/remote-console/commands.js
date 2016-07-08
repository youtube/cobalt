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

var scripts = [];
var commandCallbacks = [];
var nextRequestId = 0;

function help(command) {
  var helpString = '';
  if (command) {
    helpString = 'Command-specific help not available.\n\n';
  } else {
    // Summary help for all commands.
    helpString += 'Commands:\n\n';
    helpString += 'history: Show command history.\n';
    helpString += 'attach: Attach to remote debugger at specified host' +
                  '(default = localhost:9222).\n';
    helpString += 'detach: Close the connection to the remote debugger.\n';
    helpString += 'getScripts: List the scripts that have been parsed.\n';
    helpString += 'getScriptSource: Show the source of a specific script.\n';
    helpString += 'sendCommand: Send an arbitrary command to the debug ' +
                  'server.\n';
    helpString +=
        '\nYou are entering JavaScript, so remember to use parentheses, ' +
        'enclose string arguments in quotes, etc.\n';
  }
  printToMessageLog(messageLog.INTERACTIVE, helpString);
}

function history() {
  var history = commandInput.getHistory();
  for (var i = 0; i < history.length; i += 1) {
    printToMessageLog(messageLog.INTERACTIVE, i + ' ' + history[i]);
  }
}

// Attaches to the debugger and listens for debug events.
function attach(host) {
  printToMessageLog(messageLog.LOCAL, 'Attaching...');
  if (!host) {
    host = 'localhost:9222';
  }
  try {
    resetConnection();
    connection = new WebSocket('ws://' + host);
    connection.onopen = onOpen;
    connection.onmessage = onMessage;
    connection.onerror = onError;
    connection.onclose = onClose;
  } catch (error) {
    printToMessageLog(messageLog.ERROR, "Error: " + JSON.stringify(error));
  }
}

// Closes the current connection, if any.
function detach() {
  if (connection) {
    connection.close();
    resetConnection();
  } else {
    printToMessageLog(messageLog.WARNING, 'Not currently attached.');
  }
}

// List the parsed scripts the client has been notifed of
// via the |Debugger.scriptParsed| event. Maps the possibly very long script id
// from the debug server to a more human-readable 0-based index.
function getScripts() {
  for (var i in scripts) {
    var index = pad(i, 3);
    var scriptUrl = scripts[i].url;
    printToMessageLog(messageLog.LOCAL, index + ': ' + scriptUrl);
  }
}

// Commands; each may have an associated callback.

function getScriptSource(scriptId) {
  // If the id looks like an index into the local script array,
  // look up the real id there.
  if (scriptId >= 0 && scriptId < this.scripts.length) {
    scriptId = this.scripts[scriptId].scriptId;
  }

  var method = 'Debugger.getScriptSource'
  var params = { 'scriptId': scriptId.toString() };
  var callback = getScriptSourceCallback;
  this.sendCommand(method, params, callback);
}

function getScriptSourceCallback(result) {
  var scriptSource = result.scriptSource;
  var lines = scriptSource.split('\n');
  for (var i = 0; i < lines.length; i++) {
    var index = pad(i + 1, 4);
    printToMessageLog(messageLog.LOCAL, index + ': ' + lines[i]);
  }
}

// Generic method to send a command to the debug web server with
// optional callback.
function sendCommand(method, params, callback) {
  if (connection) {
    try {
      var request = {};
      request.id = nextRequestId++;
      request.method = method;
      request.params = params;
      var json = JSON.stringify(request);
      commandCallbacks[request.id] = callback;
      printToMessageLog(messageLog.LOCAL, json);
      connection.send(json);
    } catch (error) {
      printToMessageLog(messageLog.ERROR, error.toString());
    }
  } else {
    printToMessageLog(messageLog.WARNING,
                      'No connection - please call attach() first.');
  }
}

//--- Response and notification handlers.

function onResponse(response) {
  if (commandCallbacks[response.id]) {
    commandCallbacks[response.id](response.result);
    commandCallbacks[response.id] = null;
  }
}

function onNotification(notification) {
  if (notification.method == 'Debugger.scriptParsed') {
    this.onScriptParsed(notification.params);
  }
}

function onScriptParsed(params) {
  scripts.push(params);
}

//--- Debugger events.

function onOpen() {
  printToMessageLog(messageLog.NOTIFICATION, 'Attached.');
}

function onMessage(message) {
  var data = JSON.parse(message.data);
  if (data.error) {
    printToMessageLog(messageLog.ERROR, message.data);
  } else if (data.id != undefined && data.id != null) {
    printToMessageLog(messageLog.RESPONSE, message.data);
    onResponse(data);
  } else {
    printToMessageLog(messageLog.NOTIFICATION, message.data);
    onNotification(data);
  }
}

function onError(error) {
  printToMessageLog(messageLog.ERROR, 'Server Error: ' + JSON.stringify(error));
}

function onClose() {
  printToMessageLog(messageLog.WARNING, 'Connection closed.');
  resetConnection();
}

//--- Utils.

function pad(number, minLength) {
  var result = number.toString();
  while (result.length < minLength) {
    result = ' ' + result;
  }
  return result;
}

function resetConnection() {
  connection = null;
  scripts = [];
  nextRequestId = 0;
}