// Copyright 2015 Google Inc. All Rights Reserved.
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

// Console values constructor
function ConsoleValues() {
  // Use a prefix on all WLS keys to ensure uniqueness.
  this.KEY_PREFIX = 'cobalt.debugConsole.consoleValues';
  // Default key used for auto-save, or if the user doesn't specify another.
  this.DEFAULT_KEY = 'default';
  // Reduced space-separated list of CVal prefixes to display at start-up.
  this.DEFAULT_ACTIVE_SET =
      'Cobalt Memory.CPU Memory.MainWebModule Memory.JS Memory.Font ' +
      'Count.MainWebModule.ImageCache.Resource ' +
      'Count.MainWebModule.DOM.HtmlElement Count.MainWebModule.Layout.Box ' +
      'Event.Count.MainWebModule.KeyDown.DOM.HtmlElement.Added ' +
      'Event.Count.MainWebModule.KeyDown.Layout.Box.Created ' +
      'Event.Count.MainWebModule.KeyDown.Layout.Box.Destroyed ' +
      'Event.Duration.MainWebModule.DOM.VideoStartDelay ' +
      'Event.Duration.MainWebModule.KeyDown'

  var names = window.debugHub.getConsoleValueNames();
  this.allCVals = names.split(' ');
  // If true, we will always pull our list of CVals from
  // this.DEFAULT_ACTIVE_SET.
  this.useDefaultActiveSet = true;

  // Do a single update to initialize everything.
  this.update();
}

// Console value tree node constructor
function ConsoleValueNode() {
  this.value = null;
  this.children = null;
}

// Helper function for addToTree.
// Finds a child node with a specified name, or creates it if necessary.
ConsoleValues.prototype.findOrCreateNode = function (name, node) {
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
ConsoleValues.prototype.addToTree = function(cval, tree, prefix) {
  if (tree.children == null) {
    tree.children = new Object;
  }
  if (prefix == null) {
    prefix = '';
  }
  var components = cval.split('.');
  if (components.length == 1) {
    var node = this.findOrCreateNode(cval, tree);
    var fullName = prefix.length > 0 ? prefix + '.' + cval : cval;
    node.value = window.debugHub.getConsoleValue(fullName);
  } else {
    var newPrefix = components[0];
    var suffix = cval.substring(newPrefix.length + 1);
    node = this.findOrCreateNode(newPrefix, tree);
    if (prefix.length > 0) {
      prefix += '.';
    }
    prefix += newPrefix;
    this.addToTree(suffix, node, prefix);
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
ConsoleValues.prototype.buildTree = function(cvals) {
  var root = new ConsoleValueNode;
  for (var i = 0; i < cvals.length; i++) {
    this.addToTree(cvals[i], root, '');
  }
  return root;
}

// Helper function for consoleValueTreeToString.
// Creates the formatted string for a single console value node. In the simplest
// case, where the node has no children, this will just be the name and value.
// If the node has a single child, we call this function recursively to output
// that child, with the full name of this node as a prefix. If the node has
// multiple children, we enclose those children in braces and recursively call
// treeToString on the sub-tree.
ConsoleValues.prototype.nodeToString = function(name, node, prefix) {
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
    result += this.nodeToString(childNames[0], children[childNames[0]], name);
  } else if (numChildren > 1) {
    result += '{';
    result += this.treeToString(children, null);
    result += '}';
  }
  return result;
}

// Generates a formatted string from the parsed console value tree.
// Uses the helper function nodeToString to process each node.
// This function may in turn be called recursively by nodeToString
// to generate the string for a sub-tree (when a node has multiple children).
ConsoleValues.prototype.treeToString = function(cvalTree, prefix) {
  if (prefix == null) {
    prefix = '';
  }
  var result = '';
  var names = Object.keys(cvalTree);
  var numNames = names ? names.length : 0;
  for (var i = 0; i < numNames; i++) {
    result += this.nodeToString(names[i], cvalTree[names[i]], prefix)
    if (i < numNames - 1) {
      result += ', ';
    }
  }
  return result;
}

// Updates the complete list of registered CVals.
// If any active CVals are no longer registered, remove them from the active set.
ConsoleValues.prototype.updateRegistered = function() {
  if (this.useDefaultActiveSet) {
    this.setActiveSetToDefault();
  }

  var names = window.debugHub.getConsoleValueNames();
  this.allCVals = names.split(' ');
  for (var i = 0; i < this.activeCVals.length; i++) {
    if (this.allCVals.indexOf(this.activeCVals[i]) < 0) {
      this.activeCVals.splice(i, 1);
    }
  }
}

// Updates the tree.
ConsoleValues.prototype.update = function() {
  this.updateRegistered();
  this.cvalTree = this.buildTree(this.activeCVals);
}

// Creates a formatted string from the current tree.
ConsoleValues.prototype.toString = function() {
  if (this.cvalTree && this.cvalTree.children) {
    return this.treeToString(this.cvalTree.children, null);
  } else {
    return 'No CVals to display.';
  }
}

// Lists all registered cvals
ConsoleValues.prototype.listAll = function() {
  var result = '';
  for (var i = 0; i < this.allCVals.length; i++) {
    result += this.allCVals[i] + '\n';
  }
  return result;
}

// Adds a single CVal to the active set.
ConsoleValues.prototype.addSingleActive = function(cval) {
  if (this.activeCVals.indexOf(cval) < 0) {
    this.activeCVals.push(cval);
    return 'Added cval "' + cval + '" to display list.';
  } else {
    return '"' + cval + '" already in display list.';
  }
}

// Removes a single CVal from the active set.
ConsoleValues.prototype.removeSingleActive = function(cval) {
  var idx = this.activeCVals.indexOf(cval);
  if (idx >= 0) {
    this.activeCVals.splice(idx, 1);
    return 'Removed cval "' + cval + '" from display list.';
  } else {
    return '"' + cval + '" not in display list.';
  }
}

// Adds/removes one or more cvals from the active set.
// Each console value is matched against the space-separated list of prefixes
// and the specified method run on each match.
ConsoleValues.prototype.updateActiveSet = function(onMatch, prefixList) {
  var result = '';
  var prefixesToMatch = prefixList.split(' ');
  for (var j = 0; j < prefixesToMatch.length; j++) {
    var prefix = prefixesToMatch[j];
    var found = 0;
    for (var i = 0; i < this.allCVals.length; i++) {
      if (this.allCVals[i].indexOf(prefix) == 0) {
        found += 1;
        result += onMatch.call(this, this.allCVals[i]) + '\n';
      }
    }
    if (found == 0) {
      result += '"' + prefix + '" matched no registed cvals.\n';
    }
  }
  this.activeCVals.sort();
  return result;
}

// Saves the current active set using the web local storage.
ConsoleValues.prototype.saveActiveSet = function(key) {
  if (this.useDefaultActiveSet) {
    // If we are using the default CVals and we go to save our CVal set, lock
    // it in to the currently displayed list.
    this.updateRegistered();
    this.useDefaultActiveSet = false;
  }

  if (!key || !key.length) {
    key = this.DEFAULT_KEY;
  }
  var longKey = this.KEY_PREFIX + '.' + key;
  var value = '';
  for (var i = 0; i < this.activeCVals.length; i++) {
    value += this.activeCVals[i];
    if (i < this.activeCVals.length - 1) {
      value += ' ';
    }
  }
  window.localStorage.setItem(longKey, value);
  return 'Stored current CVal display set to "' + longKey + '"';
}

// Loads an active set from web local storage.
ConsoleValues.prototype.loadActiveSet = function(key) {
  if (!key || !key.length) {
    key = this.DEFAULT_KEY;
  }
  var longKey = this.KEY_PREFIX + '.' + key;
  var value = window.localStorage.getItem(longKey);
  if (value && value.length > 0) {
    // If we load CVals from disk, we no longer use the default set.
    this.useDefaultActiveSet = false;
    this.activeCVals = value.split(' ');
    return 'Loaded CVal display set from "' + longKey + '"';
  } else {
    return 'Could not load CVal display set from "' + longKey + '"';
  }
}

// Sets the CVal active set to the set of CVals the match the default prefixes.
// Any of the registered console values that match one of the space-separated
ConsoleValues.prototype.setActiveSetToDefault = function() {
  this.activeCVals = [];
  this.updateActiveSet(this.addSingleActive, this.DEFAULT_ACTIVE_SET);
}

// Adds one or more CVals to the active list.
// Any of the registered console values that match one of the space-separated
// set of prefixes will be added to the active set.
ConsoleValues.prototype.addActive = function(prefixesToMatch) {
  if (this.useDefaultActiveSet) {
    // If we modify our list of CVals, we should no longer rely on the defaults.
    this.updateRegistered();
    this.useDefaultActiveSet = false;
  }
  return this.updateActiveSet(this.addSingleActive, prefixesToMatch);
}

// Removes one or more CVals from the active list.
// Any of the registered console values that match one of the space-separated
// set of prefixes will be removed from the active set.
ConsoleValues.prototype.removeActive = function(prefixesToMatch) {
  if (this.useDefaultActiveSet) {
    // If we modify our list of CVals, we should no longer rely on the defaults.
    this.updateRegistered();
    this.useDefaultActiveSet = false;
  }
  return this.updateActiveSet(this.removeSingleActive, prefixesToMatch);
}
