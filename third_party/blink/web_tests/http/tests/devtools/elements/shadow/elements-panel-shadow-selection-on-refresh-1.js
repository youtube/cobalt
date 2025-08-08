// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that elements panel preserves selected shadow DOM node on page refresh.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('../resources/elements-panel-shadow-selection-on-refresh.html');

  TestRunner.runTestSuite([
    function setup(next) {
      Common.Settings.settingForTest('showUAShadowDOM').set(true);
      ElementsTestRunner.expandElementsTree(next);
    },

    function testOpenShadowRoot(next) {
      ElementsTestRunner.findNode(isOpenShadowRoot, ElementsTestRunner.selectReloadAndDump.bind(null, next));
    },

    function testClosedShadowRoot(next) {
      ElementsTestRunner.findNode(isClosedShadowRoot, ElementsTestRunner.selectReloadAndDump.bind(null, next));
    },
  ]);

  function isOpenShadowRoot(node) {
    return node && node.shadowRootType() === SDK.DOMModel.DOMNode.ShadowRootTypes.Open;
  }

  function isClosedShadowRoot(node) {
    return node && node.shadowRootType() === SDK.DOMModel.DOMNode.ShadowRootTypes.Closed;
  }
})();
