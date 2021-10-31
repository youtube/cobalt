// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Attach methods to handle commands in the 'CSS' devtools domain.
// https://chromedevtools.github.io/devtools-protocol/tot/CSS
var commands = debugBackend.CSS = {};

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#method-getComputedStyleForNode
commands.getComputedStyleForNode = function(params) {
  var node = debugBackend.DOM._findNode(params);
  var result = {};
  var nodeStyle = window.getComputedStyle(node);
  result.computedStyle = _asArray(nodeStyle).map(
      property => new devtools.CSSComputedStyleProperty(nodeStyle, property));
  return JSON.stringify(result);
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#method-getInlineStylesForNode
commands.getInlineStylesForNode = function(params) {
  var node = debugBackend.DOM._findNode(params);
  var result = {};
  result.inlineStyle = new devtools.CSSStyle(node.style);
  return JSON.stringify(result);
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#method-getMatchedStylesForNode
commands.getMatchedStylesForNode = function(params) {
  var node = debugBackend.DOM._findNode(params);
  if (!node) {
    return JSON.stringify({error: {message: 'Invalid Node'}});
  }

  var result = {};
  result.matchedCSSRules = _matchedRules(node);
  result.inlineStyle = new devtools.CSSStyle(node.style);

  result.inherited = [];
  node = node.parentElement;
  while (node) {
    result.inherited.push(new devtools.InheritedStyleEntry(node));
    node = node.parentElement;
  }

  return JSON.stringify(result);
}

var _matchedRules = function(node) {
  return _asArray(debugBackend.nativeCssAgent.getMatchingCSSRules(node)).map(
      cssRule => new devtools.RuleMatch(cssRule, node));
}

// Copy an array-like sequence into a proper JavaScript Array.
var _asArray = sequence => Array.prototype.slice.call(sequence);

// Namespace for constructors of types defined in the Devtools protocol.
var devtools = {};

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-RuleMatch
devtools.RuleMatch = function(cssRule, node) {
  this.rule = new devtools.CSSRule(cssRule);
  this.matchingSelectors = [];

  var selectors = this.rule.selectorList.selectors;
  for (var i = 0; i < selectors.length; i++) {
    if (node.matches(selectors[i].text)) {
      this.matchingSelectors.push(i);
    }
  }
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-CSSRule
devtools.CSSRule = function(cssRule) {
  // TODO: this.styleSheetId
  this.origin = 'regular'; // TODO
  this.selectorList = new devtools.SelectorList(cssRule.selectorText);
  if (cssRule.type === CSSRule.STYLE_RULE) {
    this.style = new devtools.CSSStyle(cssRule.style);
  }
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-SelectorList
devtools.SelectorList = function(cssSelectorText) {
  this.text = cssSelectorText;
  this.selectors = cssSelectorText.split(',').map(
      selector => new devtools.Value(selector.trim()));
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-Value
devtools.Value = function(value) {
  this.text = value;
  // TODO: this.range
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-CSSStyle
devtools.CSSStyle = function(cssStyleDecl) {
  this.shorthandEntries = [];  // TODO
  this.text = cssStyleDecl.cssText;
  this.cssProperties = _asArray(cssStyleDecl).map(
      property => new devtools.CSSProperty(cssStyleDecl, property));
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-CSSProperty
devtools.CSSProperty = function(cssStyleDecl, property) {
  this.name = property;
  this.value = cssStyleDecl.getPropertyValue(property);
  // this.important = cssStyleDecl.getPropertyPriority(property) === 'important';
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-CSSComputedStyleProperty
devtools.CSSComputedStyleProperty = function(cssStyleDecl, property) {
  this.name = property;
  this.value = cssStyleDecl.getPropertyValue(property);
}

// https://chromedevtools.github.io/devtools-protocol/tot/CSS#type-InheritedStyleEntry
devtools.InheritedStyleEntry = function(node) {
  this.inlineStyle = new devtools.CSSStyle(node.style);
  this.matchedCSSRules = _matchedRules(node);
}

// TODO: Pass debugBackend from C++ instead of getting it from the window.
})(window.debugBackend);
