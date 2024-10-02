// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Ensure that if a file that should be ignored is changed on the filesystem it does not propogate events.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  TestRunner.addResult('Creating filesystem');
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  await fs.reportCreatedPromise();

  Persistence.isolatedFileSystemManager.addEventListener(
      Persistence.IsolatedFileSystemManager.Events.FileSystemFilesChanged, event => {
        TestRunner.addResult('Created Files:');
        for (var createdFiles of event.data.added.valuesArray())
          TestRunner.addResult(createdFiles);
        TestRunner.addResult('');

        TestRunner.addResult('Changed Files:');
        for (var createdFiles of event.data.changed.valuesArray())
          TestRunner.addResult(createdFiles);
        TestRunner.addResult('');
      });

  TestRunner.addResult('Creating Files');
  Persistence.isolatedFileSystemManager.workspaceFolderExcludePatternSetting().set('[iI]gnored');

  TestRunner.addResult('Creating "ignoredFile"');
  var ignoredFile = fs.addFile('ignoredFile', 'content');
  TestRunner.addResult('Creating "alsoIgnoredFile"');
  var alsoIgnoredFile = fs.addFile('alsoIgnoredFile', 'content');
  TestRunner.addResult('Creating "friendlyFile"');
  var friendlyFile = fs.addFile('friendlyFile', 'content');

  TestRunner.addResult('Modifying "ignoredFile"');
  ignoredFile.setContent('content2');
  TestRunner.addResult('Modifying "alsoIgnoredFile"');
  alsoIgnoredFile.setContent('content2');
  TestRunner.addResult('Modifying "friendlyFile"');
  friendlyFile.setContent('content2');

  TestRunner.completeTest();
})();
