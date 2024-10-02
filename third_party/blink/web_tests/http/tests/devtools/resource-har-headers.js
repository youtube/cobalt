// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Tests the nondeterministic bits of HAR conversion via the magic of hard-coded values.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadTestModule('network_test_runner');

  function visibleNewlines(s) {
    return s.replace(/\r/, '\\r').replace(/\n/, '\\n');
  }

  function setRequestValues(request) {
    request.setRequestHeaders([{name: 'Request', value: 'request-value'}]);
    request.setRequestHeadersText('GET http://example.com/inspector-test.js HTTP/1.1\r\nRequest: headers-text');

    request.responseHeaders = [{name: 'Response', value: 'response-value'}];
    request.responseHeadersText = 'HTTP/1.1 200 OK\r\nResponse: headers-text';

    request.requestMethod = 'GET';
    request.mimeType = 'text/html';
    request.statusCode = 200;
    request.statusText = 'OK';
    request.resourceSize = 1000;
    request.setTransferSize(539);  // 39 = header size at the end of the day
    request.setPriority('VeryHigh');
    request.setResourceType(Common.resourceTypes.Fetch);

    // sample timing values used here are copied from a real request
    request.setIssueTime(357904.060558);
    request.endTime = 357904.867763;
    request.timing = {
      'requestTime': 357904.061527,
      'proxyStart': 1.68,
      'proxyEnd': 2.432,
      'dnsStart': 2.837,
      'dnsEnd': 86.913,
      'connectStart': 86.913,
      'connectEnd': 231.96,
      'sslStart': 133.24,
      'sslEnd': 231.942,
      'workerStart': -1,
      'workerReady': -1,
      'sendStart': 232.218,
      'sendEnd': 232.515,
      'pushStart': 0,
      'pushEnd': 0,
      'receiveHeadersEnd': 800.997
    };
  }

  const fakeInitiator = {
    type: 'parser',
    url: 'http://example.com/inspector-test.js',
    lineNumber: 117
  };
  var testRequest = SDK.NetworkRequest.create(
      'testRequest', 'http://example.com/inspector-test.js',
      'http://example.com/fake-document-url', 1, 1, fakeInitiator);
  setRequestValues(testRequest);
  var headersText = testRequest.requestHeadersText();
  var requestResults = {
    'request': {
      'headers': testRequest.requestHeaders(),
      'headersText': visibleNewlines(headersText),
      'headersSize': headersText.length,
    },
    'response': {
      'headers': testRequest.responseHeaders,
      'headersText': visibleNewlines(testRequest.responseHeadersText),
      'headersSize': testRequest.responseHeadersText.length,
      'resourceSize': testRequest.resourceSize,
      'transferSize': testRequest.transferSize
    }
  };
  TestRunner.addObject(requestResults, {}, '', 'Resource:');

  var stillNondeterministic = {
    'startedDateTime': 'formatAsTypeName',
    'time': 'formatAsTypeName',
    '_transferSize': 'formatAsTypeName',
    '_error': 'skip'
  };
  var har = await NetworkTestRunner.buildHARLogEntry(testRequest);
  TestRunner.addObject(har, stillNondeterministic, '', 'HAR:');
  TestRunner.completeTest();
})();
