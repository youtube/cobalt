// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel preserves selected shadow DOM node on page refresh.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('../resources/elements-panel-shadow-selection-on-refresh.html');

  TestRunner.runTestSuite([
    function setup(next) {
      Common.settingForTest('showUAShadowDOM').set(true);
      ElementsTestRunner.expandElementsTree(next);
    },

    function testUserAgentShadowRoot(next) {
      ElementsTestRunner.findNode(isUserAgentShadowRoot, ElementsTestRunner.selectReloadAndDump.bind(null, next));
    },

    function testOpenShadowRootChild(next) {
      ElementsTestRunner.findNode(isOpenShadowRootChild, ElementsTestRunner.selectReloadAndDump.bind(null, next));
    },
  ]);

  function isOpenShadowRoot(node) {
    return node && node.shadowRootType() === SDK.DOMNode.ShadowRootTypes.Open;
  }

  function isUserAgentShadowRoot(node) {
    return node && node.shadowRootType() === SDK.DOMNode.ShadowRootTypes.UserAgent;
  }

  function isOpenShadowRootChild(node) {
    return isOpenShadowRoot(node.parentNode);
  }
})();
