// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      body {
          margin: 0;
          padding: 0;
      }

      #inspected::before {
          content: "BEFORE"
      }
      </style>
      <div id="inspected"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function clickPseudo()
      {
          if (!window.eventSender) {
              console.log("This test requires test shell");
              return;
          }
          eventSender.mouseMoveTo(2, 2);
          eventSender.mouseDown(0);
          eventSender.mouseUp(0);
      }
  `);

  TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(inspectModeEnabled);

  function inspectModeEnabled() {
    UI.context.addFlavorChangeListener(SDK.DOMNode, selectedNodeChanged);
    TestRunner.evaluateInPage('clickPseudo()');
  }

  function selectedNodeChanged() {
    var selectedNode = ElementsTestRunner.firstElementsTreeOutline().selectedDOMNode();
    if (!selectedNode)
      TestRunner.addResult('<no selected node>');
    else
      TestRunner.addResult('Selected node pseudo type: ' + selectedNode.pseudoType());
    UI.context.removeFlavorChangeListener(SDK.DOMNode, selectedNodeChanged);
    TestRunner.completeTest();
  }
})();
