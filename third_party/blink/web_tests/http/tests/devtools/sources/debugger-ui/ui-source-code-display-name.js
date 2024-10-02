// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests UISourceCode display name.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  async function dumpUISourceCodeDisplayName(url) {
    var uiSourceCode = await SourcesTestRunner.addScriptUISourceCode(url, '');
    TestRunner.addResult(
        'UISourceCode display name for url "' + url + '" is "' +
        Sources.TabbedEditorContainer.prototype.titleForFile(uiSourceCode) + '".');
  }

  const baseURL = 'http://localhost:8080/folder/';
  await dumpUISourceCodeDisplayName(baseURL + 'filename?parameter=value&nnn=1');
  await dumpUISourceCodeDisplayName(baseURL + 'very-long-filename-123456?nn=1');
  await dumpUISourceCodeDisplayName(baseURL + 'too-long-filename-1234567890?nn=1');
  await dumpUISourceCodeDisplayName(baseURL + 'long-filename?parameter=value&nnn=1');
  await dumpUISourceCodeDisplayName(baseURL + 'too-long-filename-1234567890?parameter=value&nnn=1');
  TestRunner.completeTest();
})();
