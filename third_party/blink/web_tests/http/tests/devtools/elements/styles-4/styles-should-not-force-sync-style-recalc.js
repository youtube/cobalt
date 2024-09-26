// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspector doesn't force sync layout on operations with CSSOM.Bug 315885.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .test-0 { font-family: 'Arial'; }
      .test-1 { font-family: 'Arial'; }
      .test-2 { font-family: 'Arial'; }
      .test-3 { font-family: 'Arial'; }
      .test-4 { font-family: 'Arial'; }
      .test-5 { font-family: 'Arial'; }
      .test-6 { font-family: 'Arial'; }
      .test-7 { font-family: 'Arial'; }
      .test-8 { font-family: 'Arial'; }
      .test-9 { font-family: 'Arial'; }
      </style>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var styleElement = document.querySelector("#testSheet");
          for (var i = 0; i < 10; ++i)
              styleElement.sheet.deleteRule(0);
      }
  `);

  UI.context.setFlavor(Timeline.TimelinePanel, UI.panels.timeline);
  await PerformanceTestRunner.evaluateWithTimeline('performActions()');

  PerformanceTestRunner.mainTrackEvents().forEach(event => {
    if (event.name === TimelineModel.TimelineModel.RecordType.UpdateLayoutTree)
      TestRunner.addResult(event.name);
  });
  TestRunner.completeTest();
})();
