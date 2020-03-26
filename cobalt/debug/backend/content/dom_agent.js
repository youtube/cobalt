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
  let children;
  // Special-case the only text child - pretend the children were requested.
  if (node.firstChild && !node.firstChild.nextSibling &&
      node.firstChild.nodeType === Node.TEXT_NODE) {
    children = [node.firstChild];
  } else if (depth != 0) {  // Negative depth recurses the whole tree.
    let child_nodes = Array.from(node.childNodes);
    children = child_nodes.filter((child) => !_isWhitespace(child));

    // Since we don't report whitespace text nodes they won't be in the node
    // store and won't otherwise be observed. However, we still need to observe
    // them to report if they get set to non-whitespace.
    child_nodes.filter(_isWhitespace)
        .forEach((child) => _nodeObserver.observe(child, _observerConfig));
  } else {
    // Children not requested, so don't set |childrenReported|.
    return [];
  }

  let nodeId = _getNodeId(node);
  _nodeStore.get(nodeId).childrenReported = true;
  return children.map((child) => _getNodeWithChildren(child, depth - 1));
}

// Whether the children of a node have been reported to the frontend.
function _areChildrenReported(nodeId) {
    let nodeInfo = _nodeStore.get(nodeId);
    return nodeInfo && nodeInfo.childrenReported;
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
    _nodeObserver.observe(node, _observerConfig);
  }
  return nodeId;
}

// Removes a DOM node and its children from the internal node store.
function _removeNode(node) {
  let nodeId = _getNodeId(node);
  if (!nodeId) return;
  Array.from(node.childNodes).forEach(_removeNode);
  _nodeStore.delete(node);
  _nodeStore.delete(nodeId);
}

// Returns the node id of a DOM node if it's already known, else undefined.
function _getNodeId(node) {
  return _nodeStore.get(node);
}

// MutationObserver callback to send events when nodes are changed.
function _onNodeMutated(mutationsList) {
  for (let mutation of mutationsList) {
    if (mutation.type === 'childList') {
      _onChildListMutated(mutation);
    } else if (mutation.type === 'attributes') {
      _onAttributesMutated(mutation);
    } else if (mutation.type === 'characterData') {
      _onCharacterDataMutated(mutation);
    }
  }
}

function _onChildListMutated(mutation) {
  let parentNodeId = _getNodeId(mutation.target);
  if (!_areChildrenReported(parentNodeId)) {
    // The mutated node hasn't been expanded in the Element tree so we haven't
    // yet reported any of its children to the frontend. Just report that the
    // number of children changed, without reporting the actual child nodes.
    let params = {
      nodeId: parentNodeId,
      childNodeCount: _countChildNodes(mutation.target),
    };
    debugBackend.sendEvent('DOM.childNodeCountUpdated', JSON.stringify(params));
  } else {
    // The mutated node has already been expanded in the Element tree so the
    // frontend already knows about its children. Report the removed/inserted
    // nodes. Report removed nodes first so that replacements (e.g. setting
    // textContent) are coherent.
    Array.from(mutation.removedNodes)
        .forEach((n) => _onNodeRemoved(parentNodeId, n));
    Array.from(mutation.addedNodes)
        .forEach((n) => _onNodeInserted(parentNodeId, n));
  }
}

// Report to the frontend when a DOM node is inserted.
function _onNodeInserted(parentNodeId, node) {
  if (_isWhitespace(node)) return;

  // Forget anything we knew about an existing subtree that gets re-attached.
  _removeNode(node);

  let params = {
    parentNodeId: parentNodeId,
    previousNodeId: _getNodeId(_getPreviousSibling(node)) || 0,
    node: new devtools.Node(node),
  };
  debugBackend.sendEvent('DOM.childNodeInserted', JSON.stringify(params));
}

// Report to the frontend when a DOM node is removed.
function _onNodeRemoved(parentNodeId, node) {
  let nodeId = _getNodeId(node);
  if (!parentNodeId || !nodeId) return;

  let params = {
    parentNodeId: parentNodeId,
    nodeId: _getNodeId(node),
  };
  debugBackend.sendEvent('DOM.childNodeRemoved', JSON.stringify(params));
  _removeNode(node);
}

function _onAttributesMutated(mutation) {
  let params = {
    nodeId: _getNodeId(mutation.target),
    name: mutation.attributeName,
  };
  if (mutation.target.hasAttribute(mutation.attributeName)) {
    params.value = mutation.target.getAttribute(mutation.attributeName);
    debugBackend.sendEvent('DOM.attributeModified', JSON.stringify(params));
  } else {
    debugBackend.sendEvent('DOM.attributeRemoved', JSON.stringify(params));
  }
}

function _onCharacterDataMutated(mutation) {
  let nodeId = _getNodeId(mutation.target);
  let parentNodeId = _getNodeId(mutation.target.parentNode);
  // If a node changes to/from whitespace, treat it as inserted/removed.
  if (!nodeId) {
    _onNodeInserted(parentNodeId, mutation.target);
    return;
  } else if (_isWhitespace(mutation.target)) {
    _onNodeRemoved(parentNodeId, mutation.target);
    return;
  }

  let params = {
    nodeId: nodeId,
    characterData: mutation.target.textContent,
  };
  debugBackend.sendEvent('DOM.characterDataModified', JSON.stringify(params));
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

// Returns the non-whitespace previous sibling to the DOM node, if any.
function _getPreviousSibling(node) {
  do {
    node = node.previousSibling;
  } while(node && _isWhitespace(node));
  return node;
}

const _nodeObserver = new MutationObserver(_onNodeMutated);
const _observerConfig = {
  attributes: true,
  childList: true,
  characterData: true,
};

let _nodeStore = new Map();
let _nextNodeId = 1;

// Clear the Elements tree in DevTools. It will call DOM.getDocument in response
// to re-load the document.
// https://chromedevtools.github.io/devtools-protocol/tot/DOM#event-documentUpdated
function _documentUpdated() {
  debugBackend.sendEvent('DOM.documentUpdated', '{}');
}

// This script is injected when the 'DOM' domain is enabled. When navigating
// with DevTools connected, that happens when restoring debugger state for the
// new WebModule and its associated DebugModule along with its agents. That all
// happens before the new document is loaded, so immediately clear the old
// document from the Elements tree in DevTools while loading the new document.
_documentUpdated();

// Once the document is loaded, make DevTools reload its Elements tree again to
// pick up the new document.
document.addEventListener('load', _documentUpdated);

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
