// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests doamin filter.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  function checkSubdomains(domain) {
    TestRunner.addResult('');
    TestRunner.addResult('Domain: ' + domain);
    TestRunner.addResult('Subdomains: ' + JSON.stringify(Network.NetworkLogView.subdomains(domain)));
  }

  function checkFilter(value, domains) {
    var filter = Network.NetworkLogView.createRequestDomainFilter(value);
    TestRunner.addResult('');
    TestRunner.addResult('Filter: ' + value);
    for (var i = 0; i < domains.length; ++i)
      TestRunner.addResult('Domain \'' + domains[i] + '\' matches: ' + filter({domain: domains[i]}));
  }

  checkSubdomains('foo.bar.com');
  checkSubdomains('thumbnails');

  checkFilter('bar.com', ['foo.bar.com', 'bar.com', 'com']);
  checkFilter('*.bar.com', ['foo.bar.com', 'bar.com']);
  checkFilter('*.bar.*', ['foo.bar.com', 'baz.bar.org', 'bar.foo.net']);

  TestRunner.completeTest();
})();
