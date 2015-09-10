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

MessageBuffer.prototype.show = function(doShow) {
  var display = doShow ? 'block' : 'none';
  var elem = document.getElementById('messageLog');
  elem.style.display = display;
}

var inputText = '';
var messageBuffer = null;
var allCVals = [];
var activeCVals = [];

function createMessageBuffer(visible) {
  var messageBox = document.getElementById('messageBox');
  messageBuffer = new MessageBuffer(messageBox);
  messageBuffer.show(visible);
}

// Gets the space-separated list of console value names from Cobalt and splits
// into an array.
function getConsoleValueNames() {
  var names = window.debugHub.getConsoleValueNames();
  allCVals = names.split(' ');
  // For now, include all CVals in the active list. When we have input support
  // and command execution, the user will be able to select the active set.
  activeCVals = allCVals;
}

// Console value tree node constructor
function ConsoleValueNode() {
  this.value = null;
  this.children = null;
}

// Helper function for addConsoleValueToTree.
// Finds a child node with a specified name, or creates it if necessary.
function findOrCreateConsoleValueNode(name, node) {
  var child = null;
  if (node.children.hasOwnProperty(name)) {
    child = node.children[name];
  } else {
    child =  new ConsoleValueNode;
    node.children[name] = child;
  }
  return child;
}

// Adds a single console value to the tree.
// Console value names are split into components by dot separator. If a name
// has a single component, it is added directly at this level of the tree with
// its value, retrieved from Cobalt. If the name has multiple components, a node
// is created for the first component, then this function is called recursively
// on the remainder of the name, passing in the prefix (prior components) so the
// full name can be constructed for evaluation.
function addConsoleValueToTree(cval, tree, prefix) {
  if (tree.children == null) {
    tree.children = new Object;
  }
  if (prefix == null) {
    prefix = '';
  }
  var components = cval.split('.');
  if (components.length == 1) {
    var node = findOrCreateConsoleValueNode(cval, tree);
    var fullName = prefix.length > 0 ? prefix + '.' + cval : cval;
    node.value = window.debugHub.getConsoleValue(fullName);
  } else {
    var newPrefix = components[0];
    var suffix = cval.substring(newPrefix.length + 1);
    node = findOrCreateConsoleValueNode(newPrefix, tree);
    if (prefix.length > 0) {
      prefix += '.';
    }
    prefix += newPrefix;
    addConsoleValueToTree(suffix, node, prefix);
  }
  return tree;
}

// Creates a tree from the console values.
// Each console value name consists of one or more dot-separated components.
// The last component of each console value name corresponds to a leaf node in
// the tree; other components correspond to branch nodes.
// A display string is then created from this tree, where leaf nodes with
// a common ancestor are displayed more compactly.
// This function assumes names are sorted alphabetically.
function buildConsoleValueTree(cvals) {
  var root = new ConsoleValueNode;
  for (var i = 0; i < cvals.length; i++) {
    addConsoleValueToTree(cvals[i], root, '');
  }
  return root;
}

function printToMessageLog(severity, message) {
  messageBuffer.addMessage(severity, message);
}

function printToHud(message) {
  var elem = document.getElementById('hud');
  elem.textContent = message;
}

// Helper function for consoleValueTreeToString.
// Creates the formatted string for a single console value node. In the simplest
// case, where the node has no children, this will just be the name and value.
// If the node has a single child, we call this function recursively to output
// that child, with the full name of this node as a prefix. If the node has
// multiple children, we enclose those children in braces and recursively call
// consoleValueTreeToString on the sub-tree.
function consoleValueNodeToString(name, node, prefix) {
  var result  = '';
  var value = node.value;
  var children = node.children;
  var childNames = children != null ? Object.keys(children) : null;
  var numChildren = childNames != null ? childNames.length : 0;
  if (prefix.length > 0) {
    name = prefix + '.' + name;
  }
  if (value != null || numChildren > 1) {
    result += name + ': ';
  }
  if (value != null) {
    result += value;
    if (numChildren > 0) {
      result += ', ';
    }
  }
  if (numChildren == 1) {
    result += consoleValueNodeToString(childNames[0], children[childNames[0]],
                                       name);
  } else if (numChildren > 1) {
    result += '{';
    result += consoleValueTreeToString(children, prefix);
    result += '}';
  }
  return result;
}

// Generates a formatted string from the parsed console value tree.
// Uses the helper function consoleValueNodeToString to process each node.
// This function may in turn be called recursively by consoleValueNodeToString
// to generate the string for a sub-tree (when a node has multiple children).
function consoleValueTreeToString(cvalTree, prefix) {
  if (prefix == null) {
    prefix = '';
  }
  var result = '';
  var names = Object.keys(cvalTree);
  var numNames = names.length;
  for (var i = 0; i < numNames; i++) {
    result += consoleValueNodeToString(names[i], cvalTree[names[i]], prefix)
    if (i < numNames - 1) {
      result += ', ';
    }
  }
  return result;
}

function updateHud(time) {
  var cvalTree = buildConsoleValueTree(activeCVals);
  var cvalString = consoleValueTreeToString(cvalTree.children, null);
  printToHud(cvalString);
}

function animate(time) {
  updateHud(time);
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
  createMessageBuffer(false);
  addLogMessageCallback();
  getConsoleValueNames();
  curr = window.performance.now();
  window.requestAnimationFrame(animate);
}

window.addEventListener('load', start);
