// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the empty web animations do not show up in animation timeline.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);

  await UI.viewManager.showView('animations');
  var timeline = Animation.AnimationTimeline.instance();
  TestRunner.evaluateInPage('document.getElementById("node").animate([], { duration: 200, delay: 100 })');
  TestRunner.addSniffer(Animation.AnimationModel.prototype, 'animationStarted', animationStarted);

  function animationStarted() {
    TestRunner.addResult(timeline.previewMap.size);
    TestRunner.completeTest();
  }
})();
