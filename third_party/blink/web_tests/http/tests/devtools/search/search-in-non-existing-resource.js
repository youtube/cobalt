// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests single resource search in inspector page agent with non existing resource url does not cause a crash.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addIframe('resources/search.html');
  await TestRunner

  // This file should not match search query.
  var text = 'searchTest' +
      'UniqueString';
  ApplicationTestRunner.runAfterResourcesAreFinished(['search.js'], step2);

  async function step2() {
    var resource = Bindings.resourceForURL('http://127.0.0.1:8000/devtools/search/resources/search.js');
    var url = 'http://127.0.0.1:8000/devtools/search/resources/non-existing.js';
    var response = await TestRunner.PageAgent.invoke_searchInResource({frameId: resource.frameId, url, query: text});
    TestRunner.addResult(response.getError());
    TestRunner.completeTest();
  }
})();
