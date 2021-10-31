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

function DebugCommands(debugConsole) {
  this.debugConsole = debugConsole;
  this.consoleValues = debugConsole.consoleValues;

  this.commandRegistry = {};
  this.addBuiltinCommands();
  this.addConsoleCommands();
}

DebugCommands.prototype.addBuiltinCommands = function() {

  this.commandRegistry.cvalList = (spaceSeparatedPatternsToMatch) => {
    let result = this.consoleValues.list(spaceSeparatedPatternsToMatch);
    printToMessageLog(MessageLog.INTERACTIVE, result);
  };
  this.commandRegistry.cvalList.shortHelp =
      'List registered console values matching the space separated wildcard ' +
      'patterns (\'*\' matches any character sequence).';
  this.commandRegistry.cvalList.longHelp =
      'List registered console values that can be displayed matching ' +
      'the space separated wildcard patterns (\'*\' matches any character ' +
      'sequence).\nYou can change what subset is displayed in the HUD using ' +
      'the cvalAdd and cvalRemove debug methods.';

  this.commandRegistry.cvalAdd = (spaceSeparatedPatternsToMatch) => {
    let result = this.consoleValues.addActive(spaceSeparatedPatternsToMatch);
    printToMessageLog(MessageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  };
  this.commandRegistry.cvalAdd.shortHelp =
      'Adds one or more console values to the HUD matching the space ' +
      'separated wildcard patterns (\'*\' matches any character sequence).';
  this.commandRegistry.cvalAdd.longHelp =
      'Adds any of the registered console values (displayed with cvalList) ' +
      'to the HUD whose names match one of the specified space separated '
      'wildcard patterns (\'*\' matches any character sequence).';

  this.commandRegistry.cvalRemove = (spaceSeparatedPatternsToMatch) => {
    let result = this.consoleValues.removeActive(spaceSeparatedPatternsToMatch);
    printToMessageLog(MessageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  };
  this.commandRegistry.cvalRemove.shortHelp =
      'Removes one or more console values from the HUD matching the space ' +
      'separated wildcard patterns (\'*\' matches any character sequence).';
  this.commandRegistry.cvalRemove.longHelp =
      'Removes any of the console values displayed in the HUD ' +
      'whose names match one of the specified space-separated patterns ' +
      '(\'*\' matches any character sequence).';

  this.commandRegistry.cvalSave = (key) => {
    let result = this.consoleValues.saveActiveSet(key);
    printToMessageLog(MessageLog.INTERACTIVE, result);
  };
  this.commandRegistry.cvalSave.shortHelp =
      'Saves the current set of console values displayed in the HUD.';
  this.commandRegistry.cvalSave.longHelp =
      'Saves the set of console values currently displayed in the HUD ' +
      'to web local storage using the specified key. Saved display sets can ' +
      'be reloaded later using the cvalLoad debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  this.commandRegistry.cvalLoad = (key) => {
    let result = this.consoleValues.loadActiveSet(key);
    printToMessageLog(MessageLog.INTERACTIVE, result);
  };
  this.commandRegistry.cvalLoad.shortHelp =
      'Loads a previously stored set of console values displayed in the HUD.';
  this.commandRegistry.cvalLoad.longHelp =
      'Loads the set of console values currently displayed in the HUD ' +
      'from a set previously saved in web local storage using the cvalSave ' +
      'debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  this.commandRegistry.history = this.history.bind(this);
  this.commandRegistry.history.shortHelp = 'Display command history.';
  this.commandRegistry.history.longHelp =
      'Display a list of all previously executed commands with an '+
      'index. You can re-execute any of the commands from the ' +
      'history by typing "!" followed by the index of that command.'

  this.commandRegistry.help = this.help.bind(this);
  this.commandRegistry.help.shortHelp =
      'Display this message, or detail for a specific command.';
  this.commandRegistry.help.longHelp =
      'With no arguments, displays a summary of all commands. If the name of ' +
      'a command is specified, displays additional details about that command.';

  this.commandRegistry.dir = this.dir.bind(this);
  this.commandRegistry.dir.shortHelp =
      'Lists the properties of an object in the main web module.';
  this.commandRegistry.dir.longHelp =
      'Lists the properties of the specified object in the main web module. ' +
      'Remember to enclose the name of the object in quotes.';

  this.commandRegistry.debugger = () => {
    return this.debugConsole.debuggerClient;
  };
  this.commandRegistry.debugger.shortHelp =
      'Get the debugger client';
  this.commandRegistry.debugger.longHelp =
      'Get the debugger client. The debugger client can be used to issue ' +
      'JavaScript debugging commands to the main web module.';
}

DebugCommands.prototype.help = function(command) {
  let helpString = '';
  if (command) {
    // Detailed help on a specific command.
    if (this.commandRegistry[command]) {
      helpString = this.commandRegistry[command].longHelp;
    } else {
      helpString = 'Command "' + command + '" not found.';
    }
  } else {
    // Summary help for all commands.
    helpString = 'Cobalt Debug Console commands:\n\n';
    for (cmd in this.commandRegistry) {
      helpString += 'debug.' + cmd + '() - ';
      helpString += this.commandRegistry[cmd].shortHelp + '\n';
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

DebugCommands.prototype.history = function() {
  let history = this.debugConsole.commandInput.getHistory();
  for (let i = 0; i < history.length; i += 1) {
    printToMessageLog(MessageLog.INTERACTIVE, i + ' ' + history[i]);
  }
}

DebugCommands.prototype.dir = function(objectName) {
  let js = '(function(obj) {' +
           '  let properties = obj + "\\n";' +
           '  for (p in obj) { properties += p + "\\n"; }' +
           '  return properties;' +
           '}(' + objectName + '))';
  this.debugConsole.executeMain(js);
}

DebugCommands.prototype.addConsoleCommands = function() {
  let consoleCommands = window.debugHub.consoleCommands;
  for (let i = 0; i < consoleCommands.length; i++) {
    let c = consoleCommands[i];
    this.addOneConsoleCommand(c.command, c.shortHelp, c.longHelp);
  }
}

DebugCommands.prototype.addOneConsoleCommand = function(
    command, shortHelp, longHelp) {
  this.commandRegistry[command] = (message) => {
    window.debugHub.sendConsoleCommand(command, message);
  };
  this.commandRegistry[command].shortHelp = shortHelp;
  this.commandRegistry[command].longHelp = longHelp;
}

// Run a registered debugger command. If the command appears to be a method of
// the virtual "debug" (or shorthand "d") object it is run in the debug console
// web module and true is returned. Otherwise the command is not run and false
// is returned.
DebugCommands.prototype.executeCommand = function(command) {
  if (command.trim().indexOf('debug.') == 0 ||
      command.trim().indexOf('d.') == 0) {
    // The "debug" and "d" objects exist only while evaluating the command.
    let debug = this.commandRegistry;
    let d = this.commandRegistry;
    eval(command);
    return true;
  }
  return false;
}
