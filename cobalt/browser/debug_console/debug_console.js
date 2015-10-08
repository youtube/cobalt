var inputText = '';
var messageBuffer = null;
var consoleValues = null;
var commandInput = null;

function createMessageBuffer() {
  var messageBox = document.getElementById('messageBox');
  messageBuffer = new MessageBuffer(messageBox);
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
  messageBuffer.addMessage(severity, message);
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

function animate(time) {
  updateMode();
  updateHud(time);
  commandInput.animateBlink();
  window.requestAnimationFrame(animate);
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
    var command = commandInput.getCurrentCommand();
    printToMessageLog(window.debugHub.LOG_INFO, '> ' + command);
    window.debugHub.executeCommand(command);
    commandInput.clearAndStoreCurrentCommand();
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
  createMessageBuffer();
  showConsole(false);
  consoleValues = new ConsoleValues();
  addLogMessageCallback();
  document.addEventListener('keypress', onKeypress);
  document.addEventListener('keydown', onKeydown);
  document.addEventListener('keyup', onKeyup);
  curr = window.performance.now();
  window.requestAnimationFrame(animate);
}

window.addEventListener('load', start);

