// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests show that ssp isn't rebuild on every dom mutation\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="parent">
          <div id="foo"><div id="child"></div></div>
          <div id="sibling"><div id="child-of-sibling"></div></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function modify(id)
      {
          document.getElementById(id).setAttribute("fake", "modified");
      }
  `);

  TestRunner.runTestSuite([
    function setupTest(next) {
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('foo', didSelectElement);

      function didSelectElement() {
        TestRunner.addSniffer(
            Elements.StylesSidebarPane.prototype, 'update',
            TestRunner.addResult.bind(TestRunner, 'Requested StyleSidebarPane update'), true);
        next();
      }
    },

    function testModifySibling(next) {
      TestRunner.evaluateInPage('modify("sibling")', next);
    },

    function testModifySiblingChild(next) {
      TestRunner.evaluateInPage('modify("child-of-sibling")', next);
    },

    function testModifyParent(next) {
      TestRunner.evaluateInPage('modify("parent")', next);
    },

    function testModifyChild(next) {
      TestRunner.evaluateInPage('modify("child")', next);
    }
  ]);
})();
