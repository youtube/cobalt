// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests single resource search in inspector page agent.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addIframe('resources/search.html');

  ApplicationTestRunner.runAfterResourcesAreFinished(['search.js'], step2);
  var resource;

  async function step2() {
    resource = Bindings.resourceForURL('http://127.0.0.1:8000/devtools/search/resources/search.js');
    TestRunner.addResult(resource.url);

    // This file should not match search query.
    var text = 'searchTest' +
        'UniqueString';
    var searchMatches = await resource.searchInContent(text, false, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    // This file should not match search query.
    var text = 'searchTest' +
        'UniqueString';
    searchMatches = await resource.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await resource.searchInContent(text, false, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await resource.searchInContent(text, true, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    TestRunner.completeTest();
  }
})();
