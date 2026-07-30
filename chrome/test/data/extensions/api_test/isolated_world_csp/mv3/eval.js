// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testEval() {
  window.foo = 2;
  const expectedExceptionMessage = 'Evaluating a string as JavaScript ' +
      'violates the following Content Security Policy directive';
  // Verify `eval` throws when violating Content Security Policy.
  chrome.test.assertThrows(
      eval.bind(null, /* script */ 'window.foo = 3;'),
      new RegExp(expectedExceptionMessage));
  chrome.test.assertEq(2, window.foo);
  chrome.test.succeed();
}]);
