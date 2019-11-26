// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

(function(debugBackend) {

// Attach methods to handle commands in the 'DOM' devtools domain.
// https://chromedevtools.github.io/devtools-protocol/1-3/DOM
var commands = debugBackend.DOM = {};

// Creates and returns a new Node object corresponding to the document node,
// including its children up to a default depth.
// https://chromedevtools.github.io/devtools-protocol/1-3/DOM#method-getDocument
commands.getDocument = function(params) {
  var result = {};
  result.root = _getNodeWithChildren(document, 2);
  result.root.documentURL = document.URL;
  return JSON.stringify(result);
}

// Creates an array of Node objects corresponding to the children of the
// specified node, and returns them via an event. A depth may be specified,
// where a negative depth means to return all descendants. If no depth is
// specified, the default is 1, a single level.
// https://chromedevtools.github.io/devtools-protocol/1-3/DOM#method-requestChildNodes
commands.requestChildNodes = function(params) {
  var node = commands._findNode(params);
  var depth = params.depth || 1;
  var result = {};
  result.parentId = params.nodeId;
  result.nodes = _getChildNodes(node, depth);

  // Send the result via an event, and an empty response.
  debugBackend.sendEvent('DOM.setChildNodes', JSON.stringify(result));
  return '{}';
}

// Finds the node corresponding to a remote objectId. Also sends all nodes on
// the path from the requested one to the root as a series of setChildNodes
// events.
// https://chromedevtools.github.io/devtools-protocol/1-3/DOM#method-requestNode
commands.requestNode = function(params) {
  var node = commands._findNode(params);
  var nodeInfo = new devtools.Node(node);
  var result = {};
  result.nodeId = nodeInfo.nodeId;

  var parent = node.parentNode;
  while (parent) {
    var parentInfo = new devtools.Node(parent);
    var params = {};
    params.parentId = parentInfo.nodeId;
    params.nodes = [];
    params.nodes.push(nodeInfo);
    debugBackend.sendEvent('DOM.setChildNodes', JSON.stringify(params));
    node = parent;
    nodeInfo = parentInfo;
    parent = parent.parentNode;
  }

  return JSON.stringify(result);
}

// Returns a Runtime.RemoteObject corresponding to a node.
// https://chromedevtools.github.io/devtools-protocol/1-3/DOM#method-resolveNode
commands.resolveNode = function(params) {
  var node = commands._findNode(params);
  var result = {};
  result.object =
      JSON.parse(debugBackend.createRemoteObject(node, params.objectGroup));

  return JSON.stringify(result);
}

// Creates and returns a Node object that represents the specified node.
// Adds the node's children up to the specified depth. A negative depth will
// cause all descendants to be added.
var _getNodeWithChildren = function(node, depth) {
  var result = new devtools.Node(node);
  if (depth != 0) {
    result.children = _getChildNodes(node, depth);
  }
  return result;
}

// Creates and returns an array of Node objects corresponding to the children
// of the specified node, recursing on each on up to the specified depth.
var _getChildNodes = function(node, depth) {
  if (!node.childNodes) {
    return [];
  }

  var children = [];
  for (var i = 0; i < node.childNodes.length; i++) {
    var child = node.childNodes[i];
    if (!_nodeIsIgnorable(child)) {
      children.push(_getNodeWithChildren(child, depth - 1));
    }
  }
  return children;
}

// Finds a node specified by either nodeId or objectId (to get a node from its
// corresponding remote object). This is "exported" as a pseudo-command in the
// DOM domain for other agents to use.
commands._findNode = function(params) {
  if (params.nodeId != null) {
    return _nodeStore[params.nodeId];
  }

  if (params.objectId != null) {
    return debugBackend.lookupRemoteObjectId(params.objectId);
  }

  // Either nodeId or objectId must be specified.
  return null;
}

// Adds a node to the internal node store and returns a unique id that can
// be used to access it again.
var _addNode = function(node) {
  // If we've already added this node, then use the same nodeId.
  for (var i = 0; i < _nodeStore.length; i++) {
    if (_nodeStore[i] === node) {
      return i;
    }
  }

  var nodeId = _nextNodeId++;
  _nodeStore[nodeId] = node;
  return nodeId;
}

// Whether a node is ignorable. We ignore text nodes with white-space only
// content, as they just clutter up the DOM tree.
var _nodeIsIgnorable = function(node) {
  return node.nodeType == Node.TEXT_NODE &&
      !(/[^\t\n\r ]/.test(node.textContent));
}

var _nodeStore = [];
var _nextNodeId = 0;

// Namespace for constructors of types defined in the Devtools protocol.
var devtools = {};

// Creates a new Node object, which is the type used to return information
// about nodes to devtools. All Node objects are added to |nodeStore|,
// so they can be retrieved later via |nodeId|.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#type-Node
devtools.Node = function(node) {
  this.nodeId = _addNode(node);
  this.localName = node.nodeName;
  this.nodeName = node.nodeName;
  this.nodeType = node.nodeType;
  this.nodeValue = node.nodeValue || '';
  this.childNodeCount = _getChildNodes(node, 1).length;

  if (node.attributes) {
    this.attributes = [];
    for (var i = 0; i < node.attributes.length; i++) {
      this.attributes.push(node.attributes[i].name);
      this.attributes.push(node.attributes[i].value);
    }
  }
}

// TODO: Pass debugBackend from C++ instead of getting it from the window.
})(window.debugBackend);
