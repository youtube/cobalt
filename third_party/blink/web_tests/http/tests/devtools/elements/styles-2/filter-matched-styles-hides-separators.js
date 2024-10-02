// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that filtering in StylesSidebarPane hides sidebar separators.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #first {
          font-family: arial;
          display: block;
      }

      #second {
          font-family: helvetica;
      }

      #third {
          font-family: times;
          display: block;
      }

      #third::before {
          content: "uno-1";
      }

      #third::after {
          content: "dos-2";
          display: block;
      }

      </style>
      <div id="first">
          <div id="second">
              <div id="third">
              </div>
          </div>
      </div>
    `);

  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('third', next);
    },

    function onNodeSelected(next) {
      ElementsTestRunner.dumpRenderedMatchedStyles();
      next();
    },

    function testFilterFontFamily(next) {
      ElementsTestRunner.filterMatchedStyles('font-family');
      dumpSidebarSeparators();
      next();
    },

    function testContentProperty(next) {
      ElementsTestRunner.filterMatchedStyles('content');
      dumpSidebarSeparators();
      next();
    },

    function testDisplayProperty(next) {
      ElementsTestRunner.filterMatchedStyles('display');
      dumpSidebarSeparators();
      next();
    }
  ]);

  function dumpSidebarSeparators() {
    var separators = UI.panels.elements.stylesWidget.contentElement.querySelectorAll('.sidebar-separator');
    for (var i = 0; i < separators.length; ++i) {
      var separator = separators[i];
      var hidden = separator.classList.contains('hidden');
      var text = String.sprintf('%s %s', hidden ? '[ HIDDEN ] ' : '[ VISIBLE ]', separator.deepTextContent());
      TestRunner.addResult(text);
    }
  }
})();
