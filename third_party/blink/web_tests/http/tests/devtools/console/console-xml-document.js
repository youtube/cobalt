// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that XML document contents are logged using the correct case in the console.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.dirxml((new DOMParser()).parseFromString("<MixedCase> Test </MixedCase>", "text/xml"));
    var danglingNode = document.implementation.createDocument("", "books");
    console.dirxml(danglingNode.createElement("Book"));
  `);
  await TestRunner.showPanel('elements');

  // Warm up elements renderer.
  TestRunner.loadLegacyModule('elements').then(function() {
    ConsoleTestRunner.expandConsoleMessages(callback);
  });

  async function callback() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
