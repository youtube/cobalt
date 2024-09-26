// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that search doesn't search in binary resources.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('search');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <img src="./resources/pink.jpg">
    `);

  await TestRunner.addScriptTag('search-ignore-binary-files.js');
  SourcesTestRunner.waitForScriptSource('pink.jpg', doSearch);

  function doSearch(next) {
    var scope = new Sources.SourcesSearchScope();
    var searchConfig = new Search.SearchConfig('sources.search-in-files', 'AAAAAAA', true, false);
    SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, TestRunner.completeTest.bind(TestRunner));
  }
})();
