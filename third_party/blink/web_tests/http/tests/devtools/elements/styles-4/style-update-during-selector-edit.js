// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that modification of element styles while editing a selector does not commit the editor.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .new-class {
          color: red;
      }
      </style>
      <div id="inspected"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addStyleClass()
      {
          document.getElementById("inspected").className = "new-class";
      }
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  var seenRebuildUpdate;
  var seenAttrModified;
  var modifiedAttrNodes = [];

  function attributeChanged(event) {
    if (event.data.node === treeOutline.selectedDOMNode())
      seenAttrModified = true;
  }

  function rebuildUpdate() {
    if (UI.panels.elements.stylesWidget.node === treeOutline.selectedDOMNode())
      seenRebuildUpdate = true;
  }

  function step1() {
    TestRunner.addSniffer(Elements.StylesSidebarPane.prototype, 'doUpdate', rebuildUpdate);
    TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, attributeChanged, this);
    // Click "Add new rule".
    UI.panels.elements.stylesWidget.contentElement.querySelector('.styles-pane-toolbar')
        .shadowRoot.querySelector('.largeicon-add')
        .click();
    TestRunner.evaluateInPage('addStyleClass()', step2);
  }

  function step2() {
    if (!seenAttrModified)
      TestRunner.addResult('FAIL: AttrModified event not received.');
    else if (seenRebuildUpdate)
      TestRunner.addResult('FAIL: Styles pane updated while a selector editor was active.');
    else
      TestRunner.addResult('SUCCESS: Styles pane not updated.');
    TestRunner.completeTest();
  }
})();
