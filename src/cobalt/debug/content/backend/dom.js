/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// JavaScript functions used by the Chrome debugging protocol DOM domain:
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom

devtoolsBackend.dom = {};

// Creates and returns a new Node object corresponding to the document node,
// including its children up to a default depth.
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom#command-getDocument
devtoolsBackend.dom.getDocument = function(params) {
  var result = {};
  result.root = this.getNodeWithChildren(document, 2);
  result.root.documentURL = document.URL;
  return JSON.stringify(result);
}

// Creates an array of Node objects corresponding to the children of the
// specified node, and returns them via an event. A depth may be specified,
// where a negative depth means to return all descendants. If no depth is
// specified, the default is 1, a single level.
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom#command-requestChildNodes
devtoolsBackend.dom.requestChildNodes = function(params) {
  var node = this.findNode(params);
  var depth = params.depth || 1;
  var result = {};
  result.parentId = params.nodeId;
  result.nodes = this.getChildNodes(node, depth);

  // Send the result via an event, and an empty response.
  devtoolsBackend.sendEvent('DOM.setChildNodes', JSON.stringify(result));
  return '{}';
}

// Finds the node corresponding to a remote objectId. Also sends all nodes on
// the path from the requested one to the root as a series of setChildNodes
// events.
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom#command-requestNode
devtoolsBackend.dom.requestNode = function(params) {
  var node = this.findNode(params);
  var nodeInfo = new this.Node(node);
  var result = {};
  result.nodeId = nodeInfo.nodeId;

  var parent = node.parentNode;
  while (parent) {
    var parentInfo = new this.Node(parent);
    var params = {};
    params.parentId = parentInfo.nodeId;
    params.nodes = [];
    params.nodes.push(nodeInfo);
    devtoolsBackend.sendEvent('DOM.setChildNodes', JSON.stringify(params));
    node = parent;
    nodeInfo = parentInfo;
    parent = parent.parentNode;
  }

  return JSON.stringify(result);
}

// Returns a Runtime.RemoteObject corresponding to a node.
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom#command-resolveNode
devtoolsBackend.dom.resolveNode = function(params) {
  var node = this.findNode(params);
  var returnByValue = true;
  var result = {};
  result.object = new devtoolsBackend.runtime.RemoteObject(
        node, params.objectGroup, returnByValue);

  return JSON.stringify(result);
}

// Returns the bounding box of a node. Used for node highlighting.
devtoolsBackend.dom.getBoundingClientRect = function(params) {
  var node = this.findNode(params);
  return JSON.stringify(node.getBoundingClientRect());
}

// Creates and returns a Node object that represents the specified node.
// Adds the node's children up to the specified depth. A negative depth will
// cause all descendants to be added.
devtoolsBackend.dom.getNodeWithChildren = function(node, depth) {
  var result = new this.Node(node);
  if (depth != 0) {
    result.children = this.getChildNodes(node, depth);
  }
  return result;
}

// Creates and returns an array of Node objects corresponding to the children
// of the specified node, recursing on each on up to the specified depth.
devtoolsBackend.dom.getChildNodes = function(node, depth) {
  if (!node.childNodes) {
    return [];
  }

  var children = [];
  for (var i = 0; i < node.childNodes.length; i++) {
    var child = node.childNodes[i];
    if (!this.nodeIsIgnorable(child)) {
      children.push(this.getNodeWithChildren(child, depth - 1));
    }
  }
  return children;
}

// Finds a node specified by either nodeId or objectId (to get a node
// from its corresponding remote object).
devtoolsBackend.dom.findNode = function(params) {
  if (params.nodeId != null) {
    return this.nodeStore[params.nodeId];
  }

  if (params.objectId != null) {
    return devtoolsBackend.runtime.getObject(params.objectId);
  }

  // Either nodeId or objectId must be specified.
  return null;
}

// Adds a node to the internal node store and returns a unique id that can
// be used to access it again.
devtoolsBackend.dom.addNode = function(node) {
  // If we've already added this node, then use the same nodeId.
  for (var i = 0; i < this.nodeStore.length; i++) {
    if (this.nodeStore[i] === node) {
      return i;
    }
  }

  var nodeId = this.nextNodeId++;
  this.nodeStore[nodeId] = node;
  return nodeId;
}

// Whether a node is ignorable. We ignore text nodes with white-space only
// content, as they just clutter up the DOM tree.
devtoolsBackend.dom.nodeIsIgnorable = function(node) {
  return node.nodeType == Node.TEXT_NODE &&
      !(/[^\t\n\r ]/.test(node.textContent));
}

// Creates a new Node object, which is the type used to return information
// about nodes to devtools. All Node objects are added to |nodeStore|,
// so they can be retrieved later via |nodeId|.
devtoolsBackend.dom.Node = function(node) {
  this.nodeId = devtoolsBackend.dom.addNode(node);
  this.localName = node.nodeName;
  this.nodeName = node.nodeName;
  this.nodeType = node.nodeType;
  this.nodeValue = node.nodeValue || "";
  this.childNodeCount = node.childNodes.length;

  if (node.attributes) {
    this.attributes = [];
    for (var i = 0; i < node.attributes.length; i++) {
      this.attributes.push(node.attributes[i].name);
      this.attributes.push(node.attributes[i].value);
    }
  }
}

devtoolsBackend.dom.nodeStore = [];
devtoolsBackend.dom.nextNodeId = 0;