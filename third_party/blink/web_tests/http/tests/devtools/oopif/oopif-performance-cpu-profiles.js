// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test CPU profiles are recorded for OOPIFs.\n`);

  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.startTimeline();
  await TestRunner.navigatePromise('resources/page.html');
  await PerformanceTestRunner.stopTimeline();

  const sortedTracks = PerformanceTestRunner.timelineModel().tracks().sort((a, b) => {
    return a.url > b.url ? 1 : b.url > a.url ? -1 : 0;
  })

  for (const track of sortedTracks) {
    if (track.type !== TimelineModel.TimelineModel.TrackType.MainThread)
      continue;
    TestRunner.addResult(`name: ${track.name}`);
    TestRunner.addResult(`url: ${track.url}`);
    TestRunner.addResult(`has CpuProfile: ${track.thread.events().some(e => e.name === 'CpuProfile')}\n`);
  }

  TestRunner.completeTest();
})();
