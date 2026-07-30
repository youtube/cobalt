// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function output(msg) {
  top.postMessage({testOutput: msg}, '*');
}

chrome.devtools.inspectedWindow.eval(
    'location.href', {}, (result, exception) => {
      if (exception && exception.isError) {
        output('FAIL: expected success, got ' + exception.code);
      } else {
        output('PASS');
      }
    });
