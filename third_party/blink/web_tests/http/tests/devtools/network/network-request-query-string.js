// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests query string extraction.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  function checkURL(url) {
    var request = SDK.NetworkRequest.create(url, url, '', '', '');
    TestRunner.addResult('URL: ' + url);
    TestRunner.addResult('Query: ' + request.queryString());
    TestRunner.addResult('');
  }

  checkURL('http://webkit.org');
  checkURL('http://webkit.org?foo');
  checkURL('http://webkit.org#bar');
  checkURL('http://webkit.org?foo#bar');
  checkURL('http://webkit.org?foo=bar?baz');
  checkURL('http://webkit.org?foo?bar#baz');

  TestRunner.completeTest();
})();
