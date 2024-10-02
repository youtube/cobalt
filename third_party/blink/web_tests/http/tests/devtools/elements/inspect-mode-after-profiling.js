// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect mode works after profiling start/stop.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="width:100px;height:100px;"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function click()
      {
          var target = document.getElementById("inspected");
          var rect = target.getBoundingClientRect();
          // Simulate the mouse click over the target to trigger an event dispatch.
          eventSender.mouseMoveTo(rect.left + 10, rect.top + 10);
          eventSender.mouseDown();
          eventSender.mouseUp();
      }
  `);

  TestRunner.cpuProfilerModel.startRecording();
  TestRunner.cpuProfilerModel.stopRecording();
  TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(clickAtInspected);

  function clickAtInspected() {
    ElementsTestRunner.firstElementsTreeOutline().addEventListener(
        Elements.ElementsTreeOutline.Events.SelectedNodeChanged, dumpAndFinish);
    TestRunner.evaluateInPage('click()');
  }

  function dumpAndFinish() {
    ElementsTestRunner.firstElementsTreeOutline().removeEventListener(
        Elements.ElementsTreeOutline.Events.SelectedNodeChanged, dumpAndFinish);
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    TestRunner.addResult('Node selected: ' + selectedElement.node().getAttribute('id'));
    TestRunner.completeTest();
  }
})();
