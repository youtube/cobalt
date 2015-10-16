// Text the user has typed since last hitting Enter
var inputText = '';
// The DOM node used as a container for message nodes.
var messageLog = null;
// Stores and manipulates the set of console values.
var consoleValues = null;
// Handles command input, editing, history traversal, etc.
var commandInput = null;
// Object to store methods to be executed in the debug console.
var debug = null;
// Shorthand reference for the debug object.
var d = null;

function createMessageLog() {
  var messageContainer = document.getElementById('messageContainer');
  messageLog = new MessageLog(messageContainer);
}

function createCommandInput() {
  var inputElem = document.getElementById('in');
  this.commandInput = new CommandInput(inputElem);
}

function createConsoleValues() {
  // Create the console values and attempt to load the active set using the
  // default key. If this fails, it will leave the active set equal to the
  // all registered CVals.
  consoleValues = new ConsoleValues();
  var loadResult = consoleValues.loadActiveSet();
  printToMessageLog(messageLog.INTERACTIVE, loadResult);
}

function showBlockElem(elem, doShow) {
  if (elem) {
    var display = doShow ? 'block' : 'none';
    elem.style.display = display;
  }
}

function showConsole(doShow) {
  showBlockElem(document.getElementById('consoleFrame'), doShow);
  messageLog.setVisible(doShow);
}

function printToMessageLog(severity, message) {
  messageLog.addMessage(severity, message);
}

function showHud(doShow) {
  showBlockElem(document.getElementById('hud'), doShow);
}

function printToHud(message) {
  var elem = document.getElementById('hud');
  elem.textContent = message;
}

function updateHud(time) {
  consoleValues.update();
  var cvalString = consoleValues.toString();
  printToHud(cvalString);
}

function updateMode() {
  var mode = window.debugHub.getDebugConsoleMode();
  showConsole(mode >= window.debugHub.DEBUG_CONSOLE_ON);
  showHud(mode >= window.debugHub.DEBUG_CONSOLE_HUD);
}

// Animation callback: updates state and animated nodes.
function animate(time) {
  updateMode();
  updateHud(time);
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

// JavaScript commands executed in this (debug console) web module.
// The only commands we execute here are methods of the debug object
// (or its shorthand equivalent).
function executeDebug(command) {
  if (command.trim().indexOf('debug.') == 0 ||
      command.trim().indexOf('d.') == 0) {
    eval(command);
    return true;
  }
  return false;
}

// Execute a command as JavaScript in the main web module.
function executeMain(command) {
  var result = window.debugHub.executeJavascript(command);
  if (command.indexOf('console') == -1) {
    // Echo the output for non-console commands.
    printToMessageLog(messageLog.INFO, result);
  }
}

// Executes a command entered by the user.
// Commands are processed in the following order:
// 1. Check for an 'immediate' command, e.g. history - !n.
// 2. Execute debug JS commands in this web module.
// 3. If no matching command is found, pass to the Cobalt DebugHub.
//    DebugHub will execute any commands recognized on the C++ side,
//    or pass to the main web module to be executed as JavaScript.
function executeCommand(command) {
  if (executeImmediate(command)) {
    return;
  }
  commandInput.storeAndClearCurrentCommand();
  if (executeDebug(command)) {
    return;
  }
  executeMain(command);
}

// Executes the current command in the CommandInput object.
// Typically called when the user hits Enter.
function executeCurrentCommand() {
  var command = commandInput.getCurrentCommand();
  printToMessageLog(messageLog.INTERACTIVE, '> ' + command);
  executeCommand(command);
  printToMessageLog(messageLog.INTERACTIVE, '');
}

function onKeydown(event) {
  var key = event.key;
  if (key == 'ArrowLeft') {
    commandInput.moveCursor(-1);
  } else if (key == 'ArrowRight') {
    commandInput.moveCursor(1);
  } else if (key == 'ArrowUp') {
    commandInput.back();
  } else  if (key == 'ArrowDown') {
    commandInput.forward();
  } else if (key == 'Backspace') {
    commandInput.deleteCharBehindCursor();
  } else if (key == 'Enter') {
    executeCurrentCommand();
  } else if (key == 'PageUp') {
    messageLog.pageUp();
  } else if (key == 'PageDown') {
    messageLog.pageDown();
  }
}

function onKeyup(event) {}

function onKeypress(event) {
  var mode = window.debugHub.getDebugConsoleMode();
  if (mode >= window.debugHub.DEBUG_CONSOLE_ON) {
    event.preventDefault();
    event.stopPropagation();
    var c = event.charCode;
    // If we have a printable character, insert it; otherwise ignore.
    if (c >= 0x20 && c <= 0x7e) {
      commandInput.insertCharBehindCursor(String.fromCharCode(c));
    }
  }
}

function onLogMessage(severity, file, line, messageStart, str) {
  printToMessageLog(severity, str);
}

function addLogMessageCallback() {
  var debugHub = window.debugHub;
  if (debugHub) {
    debugHub.addLogMessageCallback(onLogMessage);
  }
}

function start() {
  createCommandInput();
  createMessageLog();
  showConsole(false);
  createConsoleValues();
  initDebugCommands();
  addLogMessageCallback();
  document.addEventListener('keypress', onKeypress);
  document.addEventListener('keydown', onKeydown);
  document.addEventListener('keyup', onKeyup);
  curr = window.performance.now();
  window.requestAnimationFrame(animate);
}

window.addEventListener('load', start);

