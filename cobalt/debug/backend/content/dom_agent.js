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
// https://chromedevtools.github.io/devtools-protocol/tot/DOM
let commands = debugBackend.DOM = {};

// Creates and returns a new devtools.Node object corresponding to the document
// DOM node, including its children up to a default depth.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#method-getDocument
commands.getDocument = function(params) {
  let result = {};
  result.root = _getNodeWithChildren(document, 2);
  result.root.documentURL = document.URL;
  return JSON.stringify(result);
}

// Creates an array of devtools.Node objects corresponding to the children of
// the DOM node specified in the command params, and returns them via an event.
// A depth may be specified, where a negative depth means to return all
// descendants. If no depth is specified, the default is 1, a single level.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#method-requestChildNodes
commands.requestChildNodes = function(params) {
  let node = commands._findNode(params);
  _reportChildren(node, params.depth)
  return '{}';
}

// Finds the node corresponding to a remote objectId. Also sends all unreported
// nodes from the root to the requested node as a series of DOM.setChildNodes
// events.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#method-requestNode
commands.requestNode = function(params) {
  let node = commands._findNode(params);
  if (!_getNodeId(node)) {
    _reportPathFromRoot(node.parentNode);
  }
  return JSON.stringify({nodeId: _getNodeId(node)});
}

// Returns a Runtime.RemoteObject corresponding to a node.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#method-resolveNode
commands.resolveNode = function(params) {
  let node = commands._findNode(params);
  let result = {};
  result.object =
      JSON.parse(debugBackend.createRemoteObject(node, params.objectGroup));

  return JSON.stringify(result);
}

// Creates and returns a devtools.Node object that represents the specified DOM
// node. Adds the node's children up to the specified depth. A negative depth
// will cause all descendants to be added. All returned children are added to
// the node store, and should be reported to the client to maintain integrity of
// the node store holding only nodes the client knows about.
function _getNodeWithChildren(node, depth) {
  let result = new devtools.Node(node);
  let children = _getChildNodes(node, depth);
  if (children.length) {
    result.children = children;
  }
  return result;
}

// Creates and returns an array of devtools.Node objects corresponding to the
// children of the specified DOM node, recursing on each on down to the
// specified depth. All returned children are added to the node store, and
// should be reported to the client to maintain integrity of the node store
// holding only nodes the client knows about.
function _getChildNodes(node, depth) {
  let children = [];
  // Special-case the only text child - pretend the children were requested.
  if (node.firstChild && !node.firstChild.nextSibling &&
      node.firstChild.nodeType === Node.TEXT_NODE) {
    children = [node.firstChild];
  } else if (depth != 0) {  // Negative depth recurses the whole tree.
    children =
      Array.from(node.childNodes).filter((child) => !_isWhitespace(child));
  }
  return children.map((child) => _getNodeWithChildren(child, depth - 1));
}

// Sends DOM.setChildNode events to report all nodes not yet known to the client
// from the root down to the specified DOM node.
function _reportPathFromRoot(node) {
  // Report nothing if we get to a disconnected root.
  if (!node) {
    return false;
  }
  // Stop recursing when we get to a node that has already been reported, and
  // report its children first before unwinding the recursion down the tree.
  if (_getNodeId(node)) {
    _reportChildren(node);
    return true;
  }
  // Recurse up first to report in top-down order, and report nothing if we
  // reached a disconnected root.
  if (!_reportPathFromRoot(node.parentNode)) {
    return false;
  }
  // All ancestors are now reported, so report the node's children.
  _reportChildren(node);
  return true;
}

// Sends a DOM.setChildNodes event reporting the children of a DOM node, and
// their children recursively to the requested depth.
function _reportChildren(node, depth) {
  let nodeId = _addNode(node);
  let children = _getChildNodes(node, depth || 1);
  let params = {
    parentId: nodeId,
    nodes: children,
  };
  debugBackend.sendEvent('DOM.setChildNodes', JSON.stringify(params));
}

// Finds a DOM node specified by either nodeId or objectId (to get a node from
// its corresponding remote object). This is "exported" as a pseudo-command in
// the DOM domain for other agents to use.
commands._findNode = function(params) {
  if (params.nodeId) {
    return _nodeStore.get(params.nodeId).node;
  }

  if (params.objectId) {
    return debugBackend.lookupRemoteObjectId(params.objectId);
  }

  // Either nodeId or objectId must be specified.
  return null;
}

// Adds a DOM node to the internal node store and returns a unique id that can
// be used to access it again. If the node is already in the node store, its
// existing id is returned.
function _addNode(node) {
  let nodeId = _getNodeId(node);
  if (!nodeId) {
    nodeId = _nextNodeId++;
    // The map goes both ways: DOM node <=> node ID.
    _nodeStore.set(nodeId, {node: node});
    _nodeStore.set(node, nodeId);
  }
  return nodeId;
}

// Returns the node id of a DOM node if it's already known, else undefined.
function _getNodeId(node) {
  return _nodeStore.get(node);
}

// Whether a DOM node is a whitespace-only text node.
// (These are not reported to the frontend.)
function _isWhitespace(node) {
  return node.nodeType === Node.TEXT_NODE &&
      !(/[^\t\n\r ]/.test(node.nodeValue));
}

// Returns the count of non-whitespace children of a DOM node.
function _countChildNodes(node) {
  let countCallback = (count, child) => count + (_isWhitespace(child) ? 0 : 1);
  return Array.from(node.childNodes || []).reduce(countCallback, 0);
}

let _nodeStore = new Map();
let _nextNodeId = 1;

// Namespace for constructors of types defined in the Devtools protocol.
let devtools = {};

// Constructor for devtools.Node object, which is the type used to return
// information about nodes to the frontend. The associated DOM node is added to
// |nodeStore| since all devtools.Node objects are expected to be reported to
// the frontend, which can reference them later via |nodeId|.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#type-Node
devtools.Node = function(node) {
  this.nodeId = _addNode(node);
  this.localName = node.nodeName;
  this.nodeName = node.nodeName;
  this.nodeType = node.nodeType;
  this.nodeValue = node.nodeValue || '';
  this.childNodeCount = _countChildNodes(node);

  if (node.attributes) {
    this.attributes = [];
    for (let i = 0; i < node.attributes.length; i++) {
      this.attributes.push(node.attributes[i].name);
      this.attributes.push(node.attributes[i].value);
    }
  }
}

// TODO: Pass debugBackend from C++ instead of getting it from the window.
})(window.debugBackend);
