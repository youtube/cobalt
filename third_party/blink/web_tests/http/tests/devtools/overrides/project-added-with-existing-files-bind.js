// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Ensures that when a project is added with already existing files they bind.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/network/resources/empty.html');

  Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingCreated, event => {
    const binding = event.data;
    TestRunner.addResult('Bound Files:');
    TestRunner.addResult(binding.network.url() + ' <=> ' + binding.fileSystem.url());
    TestRunner.addResult('');
    TestRunner.completeTest();
  });

  const { testFileSystem } = await BindingsTestRunner.createOverrideProject('file:///tmp');
  testFileSystem.addFile('127.0.0.1%3a8000/devtools/network/resources/empty.html', 'New Content');

  BindingsTestRunner.setOverridesEnabled(true);
})();
