// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that InspectorBackendStub is catching incorrect arguments.\n`);


  console.error = function() {
    TestRunner.addResult(String.sprintf.apply(this, arguments));
  };

  TestRunner.NetworkAgent.setUserAgentOverride(1);
  TestRunner.NetworkAgent.setUserAgentOverride();
  TestRunner.NetworkAgent.setUserAgentOverride('123', 'not a function');
  TestRunner.NetworkAgent.setUserAgentOverride('123', undefined);
  TestRunner.RuntimeAgent.evaluate('true', 'test');
  TestRunner.RuntimeAgent.evaluate('true', 'test', function() {});
  TestRunner.RuntimeAgent.evaluate('true', 'test', undefined, function() {});
  TestRunner.mainTarget._router._onMessage('{"method": "wrongDomain.something-strange", "params": {}}');
  TestRunner.mainTarget._router._onMessage('{"method": "Inspector.something-strange", "params": {}}');

  TestRunner.completeTest();
})();
