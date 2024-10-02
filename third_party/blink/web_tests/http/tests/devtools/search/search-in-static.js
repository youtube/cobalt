// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests static content provider search.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addIframe('resources/search.html');

  ApplicationTestRunner.runAfterResourcesAreFinished(['search.js'], step2);
  var resource;
  var staticContentProvider;

  function step2() {
    resource = Bindings.resourceForURL('http://127.0.0.1:8000/devtools/search/resources/search.js');
    resource.requestContent().then(step3);
  }

  async function step3() {
    staticContentProvider = TextUtils.StaticContentProvider.fromString('', Common.resourceTypes.Script, resource.content);
    TestRunner.addResult(resource.url);

    var text = 'searchTestUniqueString';
    var searchMatches = await staticContentProvider.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = 'searchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, false, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, true, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    TestRunner.completeTest();
  }
})();
