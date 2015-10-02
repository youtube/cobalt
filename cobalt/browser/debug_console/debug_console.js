var inputText = '';
var messageBuffer = null;
var consoleValues = null;
var mode = window.debugHub.DEBUG_CONSOLE_HUD;

function createMessageBuffer() {
  var messageBox = document.getElementById('messageBox');
  messageBuffer = new MessageBuffer(messageBox);
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
  window.requestAnimationFrame(animate);
}

function onKeydown(event) {}

function onKeyup(event) {}

function onKeypress(event) {
  var mode = window.debugHub.getDebugConsoleMode();
  if (mode >= window.debugHub.DEBUG_CONSOLE_ON) {
    event.preventDefault();
    event.stopPropagation();
    var i = document.querySelector('#in');
    var k = event.charCode;
    if (k == 8) {
      inputText = inputText.substring(0, inputText.length - 1);
    } else if (k == 13) {
      window.debugHub.executeCommand(inputText);
      inputText = "";
    } else if (k >= 0x20 && k < 0x7e) {
      inputText += String.fromCharCode(k);
    }
    i.textContent = '> ' + inputText + '_';
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

