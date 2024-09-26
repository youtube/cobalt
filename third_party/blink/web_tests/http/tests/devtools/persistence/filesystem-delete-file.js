// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that uiSourceCode.delete actually deltes file from IsolatedFileSystem.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'script.js': {content: 'testme'},
    'bar.js': {content: 'another'},
  });
  fs.reportCreated(function() {});
  TestRunner.waitForUISourceCode('script.js').then(onUISourceCode);

  function onUISourceCode(uiSourceCode) {
    TestRunner.addResult('BEFORE:\n' + 'file://' + fs.dumpAsText());
    uiSourceCode.remove();
    TestRunner.addResult('\nAFTER:\n' + 'file://' + fs.dumpAsText());
    TestRunner.completeTest();
  }
})();
