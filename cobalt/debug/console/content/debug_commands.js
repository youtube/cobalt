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

function initDebugCommands() {
  let debugCommands = {};

  debugCommands.cvalList = function() {
    var result = consoleValues.listAll();
    printToMessageLog(MessageLog.INTERACTIVE, result);
  }
  debugCommands.cvalList.shortHelp = 'List all registered console values.';
  debugCommands.cvalList.longHelp =
      'List all registered console values that can be displayed.\n' +
      'You can change what subset is displayed in the HUD using ' +
      'the cvalAdd and cvalRemove debug methods.';

  debugCommands.cvalAdd = function(substringToMatch) {
    var result = consoleValues.addActive(substringToMatch);
    printToMessageLog(MessageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  }
  debugCommands.cvalAdd.shortHelp =
      'Adds one or more consoles value to the HUD.';
  debugCommands.cvalAdd.longHelp =
      'Adds any of the registered consolve values (displayed with cvalList) ' +
      'to the HUD whose name matches one of the specified space-separated '
      'prefixes.';

  debugCommands.cvalRemove = function(substringToMatch) {
    var result = consoleValues.removeActive(substringToMatch);
    printToMessageLog(MessageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  }
  debugCommands.cvalRemove.shortHelp =
      'Removes one or more consoles value from the HUD.';
  debugCommands.cvalRemove.longHelp =
      'Removes any of the consolve values displayed in the HUD ' +
      'whose name matches one of the specified space-separated prefixes.';

  debugCommands.cvalSave = function(key) {
    var result = consoleValues.saveActiveSet(key);
    printToMessageLog(MessageLog.INTERACTIVE, result);
  }
  debugCommands.cvalSave.shortHelp =
      'Saves the current set of console values displayed in the HUD.';
  debugCommands.cvalSave.longHelp =
      'Saves the set of console values currently displayed in the HUD ' +
      'to web local storage using the specified key. Saved display sets can ' +
      'be reloaded later using the cvalLoad debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  debugCommands.cvalLoad = function(key) {
    var result = consoleValues.loadActiveSet(key);
    printToMessageLog(MessageLog.INTERACTIVE, result);
  }
  debugCommands.cvalLoad.shortHelp =
      'Loads a previously stored set of console values displayed in the HUD.';
  debugCommands.cvalLoad.longHelp =
      'Loads the set of console values currently displayed in the HUD ' +
      'from a set previously saved in web local storage using the cvalSave ' +
      'debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  debugCommands.history = history;
  debugCommands.history.shortHelp = 'Display command history.';
  debugCommands.history.longHelp =
      'Display a list of all previously executed commands with an '+
      'index. You can re-execute any of the commands from the ' +
      'history by typing "!" followed by the index of that command.'

  debugCommands.help = help.bind(this, debugCommands);
  debugCommands.help.shortHelp =
      'Display this message, or detail for a specific command.';
  debugCommands.help.longHelp =
      'With no arguments, displays a summary of all commands. If the name of ' +
      'a command is specified, displays additional details about that command.';

  debugCommands.dir = dir;
  debugCommands.dir.shortHelp =
      'Lists the properties of an object in the main web module.';
  debugCommands.dir.longHelp =
      'Lists the properties of the specified object in the main web module. ' +
      'Remember to enclose the name of the object in quotes.';

  debugCommands.debugger = function() {
    return debuggerClient;
  }
  debugCommands.debugger.shortHelp =
      'Get the debugger client';
  debugCommands.debugger.longHelp =
      'Get the debugger client. The debugger client can be used to issue ' +
      'JavaScript debugging commands to the main web module.';

  addConsoleCommands(debugCommands);

  return debugCommands;
}

function help(debugCommands, command) {
  var helpString = '';
  if (command) {
    // Detailed help on a specific command.
    if (debugCommands[command]) {
      helpString = debugCommands[command].longHelp;
    } else {
      helpString = 'Command "' + command + '" not found.';
    }
  } else {
    // Summary help for all commands.
    helpString = 'Cobalt Debug Console commands:\n\n';
    for (cmd in debugCommands) {
      helpString += 'debug.' + cmd + '() - ';
      helpString += debugCommands[cmd].shortHelp + '\n';
    }
    helpString +=
        '\nYou are entering JavaScript, so remember to use parentheses, ' +
        'enclose string arguments in quotes, etc.\n' +
        'You can use "d." as a shorthand for "debug."\n' +
        'All other text will be executed as JavaScript in the main web ' +
        'module.\n';
  }
  printToMessageLog(MessageLog.INTERACTIVE, helpString);
}

function history() {
  var history = commandInput.getHistory();
  for (var i = 0; i < history.length; i += 1) {
    printToMessageLog(MessageLog.INTERACTIVE, i + ' ' + history[i]);
  }
}

function dir(objectName) {
  var js = '(function(obj) {' +
           '  var properties = obj + "\\n";' +
           '  for (p in obj) { properties += p + "\\n"; }' +
           '  return properties;' +
           '}(' + objectName + '))';
  executeMain(js);
}

function addConsoleCommands(debugCommands) {
  var consoleCommands = window.debugHub.consoleCommands;
  for (var i = 0; i < consoleCommands.length; i++) {
    var c = consoleCommands[i];
    addOneConsoleCommand(debugCommands, c.command, c.shortHelp, c.longHelp);
  }
}

function addOneConsoleCommand(debugCommands, command, shortHelp, longHelp) {
  debugCommands[command] = function(message) {
    window.debugHub.sendConsoleCommand(command, message);
  }
  debugCommands[command].shortHelp = shortHelp;
  debugCommands[command].longHelp = longHelp;
}
