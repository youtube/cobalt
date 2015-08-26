// First, extremely basic, version of a class to manage message buffering.
function MessageBuffer(messageBox) {
  this.maxBufferSize = 50;
  this.messageBox = messageBox;
}

MessageBuffer.prototype.addMessage = function(severity, message) {
  // Create the new text element with the message.
  var elem = document.createElement('div');
  var text = document.createTextNode(message);

  if (severity == window.debugHub.LOG_INFO) {
    elem.style.color = '#FFFFFF';
  } else if (severity == window.debugHub.LOG_WARNING) {
    elem.style.color = '#FFFF80';
  } else if (severity == window.debugHub.LOG_ERROR ||
             severity == window.debugHub.LOG_ERROR_REPORT ||
             severity == window.debugHub.LOG_FATAL) {
    elem.style.color = '#FF9080';
  } else {
    elem.style.color = '#FFFFFF';
  }
  elem.appendChild(text);

  // Remove any older elements that exceed the buffer size and
  // add the new element.
  while (this.messageBox.childElementCount >= this.maxBufferSize) {
    this.messageBox.removeChild(this.messageBox.firstChild);
  }
  this.messageBox.appendChild(elem);
}

var x = 400;
var y = 400;
var vx = 5;
var vy = 3;
var curr = 0;
var inputText = '';
var messageBuffer = null;

function createMessageBuffer() {
  var messageBox = document.getElementById('messageBox');
  messageBuffer = new MessageBuffer(messageBox);
}

function printToMessageLog(severity, message) {
  messageBuffer.addMessage(severity, message);
}

function printToHud(message) {
  var elem = document.getElementById('hud');
  elem.innerHTML = '<p>' + message;
}

function printTime(time) {
  var dt = time - curr;
  if (dt > 0) {
    printToHud('layout refresh rate = ' + 1000.0/dt);
  }
  curr = time;
}

function movePirateFlag() {
  var elem = document.getElementById('pirate');
  if (x < 1 || x > window.innerWidth - 235) {
    vx *= -1;
  }
  if (y < 1 || y > window.innerHeight - 235) {
    vy *= -1;
  }
  x += vx;
  y += vy;
  elem.style.left = x+'px';
  elem.style.top = y+'px';
}

function animate(time) {
  printTime(time);
  movePirateFlag();
  window.requestAnimationFrame(animate);
}

var onKeyDown = function(event) {
  console.log('Got key press!');
  event.preventDefault();
  event.stopPropagation();
  var i = document.querySelector('#in');
  var k = event.keyCode;
  if (k == 8) {
    inputText = inputText.substring(0, inputText.length - 1);
  } else if (k >= 0x20 && k < 0x7e) {
    inputText += String.fromCharCode(k);
  }
  i.textContent = inputText;
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
  addLogMessageCallback();
  curr = window.performance.now();
  window.requestAnimationFrame(animate);
}

window.addEventListener('load', start);
