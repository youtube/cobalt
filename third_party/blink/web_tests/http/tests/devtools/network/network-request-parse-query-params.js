// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests query string parsing.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  function checkQuery(query) {
    var url = 'http://webkit.org?' + query;
    var request = SDK.NetworkRequest.create(url, url, '', '', '');
    TestRunner.addResult('Query: ' + request.queryString());
    var params = request.queryParameters;
    TestRunner.addResult('Parameters: ');
    for (var i = 0; i < params.length; ++i) {
      var nameValue = params[i];
      TestRunner.addResult('  ' + nameValue.name + ': ' + nameValue.value);
    }
    TestRunner.addResult('');
  }

  checkQuery('a=b&c=d');
  checkQuery('a&b');
  checkQuery('a=b=c=d');

  TestRunner.completeTest();
})();
