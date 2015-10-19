function initDebugCommands() {
  debug = new Object();
  d = debug;

  debug.cvalList = function() {
    var result = consoleValues.listAll();
    printToMessageLog(messageLog.INTERACTIVE, result);
  }
  debug.cvalList.shortHelp = 'List all registered console values.';
  debug.cvalList.longHelp =
      'List all registered console values that can be displayed.\n' +
      'You can change what subset is displayed in the HUD using ' +
      'the cvalAdd and cvalRemove debug methods.';

  debug.cvalAdd = function(substringToMatch) {
    var result = consoleValues.addActive(substringToMatch);
    printToMessageLog(messageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  }
  debug.cvalAdd.shortHelp = 'Adds one or more consoles value to the HUD.';
  debug.cvalAdd.longHelp =
      'Adds any of the registered consolve values (displayed with cvalList) ' +
      'to the HUD whose name matches one of the specified space-separated '
      'prefixes.';

  debug.cvalRemove = function(substringToMatch) {
    var result = consoleValues.removeActive(substringToMatch);
    printToMessageLog(messageLog.INTERACTIVE, result);
    // After each change, save the active set with the default key.
    this.cvalSave();
  }
  debug.cvalRemove.shortHelp =
      'Removes one or more consoles value from the HUD.';
  debug.cvalRemove.longHelp =
      'Removes any of the consolve values displayed in the HUD ' +
      'whose name matches one of the specified space-separated prefixes.';

  debug.cvalSave = function(key) {
    var result = consoleValues.saveActiveSet(key);
    printToMessageLog(messageLog.INTERACTIVE, result);
  }
  debug.cvalSave.shortHelp =
      'Saves the current set of console values displayed in the HUD.';
  debug.cvalSave.longHelp =
      'Saves the set of console values currently displayed in the HUD ' +
      'to web local storage using the specified key. Saved display sets can ' +
      'be reloaded later using the cvalLoad debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  debug.cvalLoad = function(key) {
    var result = consoleValues.loadActiveSet(key);
    printToMessageLog(messageLog.INTERACTIVE, result);
  }
  debug.cvalLoad.shortHelp =
      'Loads a previously stored set of console values displayed in the HUD.';
  debug.cvalLoad.longHelp =
      'Loads the set of console values currently displayed in the HUD ' +
      'from a set previously saved in web local storage using the cvalSave ' +
      'debug method and the same key.\n' +
      'If no key is specified, uses a default value.';

  debug.history = history;
  debug.history.shortHelp = 'Display command history.';
  debug.history.longHelp =
      'Display a list of all previously executed commands with an '+
      'index. You can re-execute any of the commands from the ' +
      'history by typing "!" followed by the index of that command.'

  debug.help = help;
  debug.help.shortHelp = 'Display this message, or detail for a specific command.';
  debug.help.longHelp =
      'With no arguments, displays a summary of all commands. If the name of ' +
      'a command is specified, displays additional details about that command.';

  debug.dir = dir;
  debug.dir.shortHelp =
      'Lists the properties of an object in the main web module.';
  debug.dir.longHelp =
      'Lists the properties of the specified object in the main web module. ' +
      'Remember to enclose the name of the object in quotes.';
}

function help(command) {
  var helpString = '';
  if (command) {
    // Detailed help on a specific command.
    if (debug[command]) {
      helpString = debug[command].longHelp;
    } else {
      helpString = 'Command "' + command + '" not found.';
    }
  } else {
    // Summary help for all commands.
    helpString = 'Cobalt Debug Console commands:\n\n';
    for (cmd in debug) {
      helpString += 'debug.' + cmd + '() - ';
      helpString += debug[cmd].shortHelp + '\n';
    }
    helpString +=
        '\nYou are entering JavaScript, so remember to use parentheses, ' +
        'enclose string arguments in quotes, etc.\n' +
        'You can use "d." as a shorthand for "debug."\n' +
        'All other text will be executed as JavaScript in the main web ' +
        'module.\n';
  }
  printToMessageLog(messageLog.INTERACTIVE, helpString);
}

function history() {
  var history = commandInput.getHistory();
  for (var i = 0; i < history.length; i += 1) {
    printToMessageLog(messageLog.INTERACTIVE, i + ' ' + history[i]);
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
