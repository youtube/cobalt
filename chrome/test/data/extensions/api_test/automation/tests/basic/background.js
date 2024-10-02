// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.query({active: true}, function(tabs) {
  chrome.automation.getTree(function() {
    chrome.test.sendMessage('got_tree');
  });
});
