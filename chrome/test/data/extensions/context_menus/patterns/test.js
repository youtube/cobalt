// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var make_browsertest_proceed = function() {
  if (!chrome.runtime.lastError) {
    chrome.test.sendMessage('created items');
  }
};

var patterns = ['http://*.google.com/*', 'https://*.google.com/*'];

// Create one item that does have a documentUrlPattern.
var properties1 = {
  title: 'test_item1', id: 'item1', documentUrlPatterns: patterns
};

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.create(properties1);

  // Create an item that initially doesn't have a documentUrlPattern, then
  // update it, and then proceed with the c++ code in the browser test.
  var properties2 = { title: 'test_item2', id: 'item2' };

  var id2 = chrome.contextMenus.create(properties2,
                                       function() {
    var update_properties = { documentUrlPatterns: patterns };
    chrome.contextMenus.update(id2, update_properties,
                               make_browsertest_proceed);
})});
