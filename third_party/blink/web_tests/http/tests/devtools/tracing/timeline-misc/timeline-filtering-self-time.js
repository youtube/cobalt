// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test filtering in Bottom-Up Timeline Tree View panel.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var pid = 100;

  var testData = [
    {
      'args': {
        'data': {
          'sessionId': sessionId,
          'frames':
              [{'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 100,
    },
    {
      'name': 'top level event name',
      'ts': 1000000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1010000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'AAA'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1020000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'BBB'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1100000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1110000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'top level event name',
      'ts': 1120000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
    }
  ];

  var model = await PerformanceTestRunner.createPerformanceModelWithEvents(testData);
  const tabbedPane = UI.panels.timeline.flameChart.detailsView.tabbedPane;
  tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.BottomUp);
  const view = tabbedPane.visibleView;

  view.setModel(model, PerformanceTestRunner.mainTrack());
  view.updateContents(Timeline.TimelineSelection.fromRange(
      model.timelineModel().minimumRecordTime(),
      model.timelineModel().maximumRecordTime()));
  function printEventMessage(event, level, node) {
    const text = event.args['data'] && event.args['data']['message'] ?
        event.args['data']['message'] + ' selfTime: ' + node.selfTime :
        event.name;
    TestRunner.addResult(' '.repeat(level) + text);
  }

  async function dumpRecords() {
    await PerformanceTestRunner.walkTimelineEventTreeUnderNode(
        printEventMessage, view.root);
    TestRunner.addResult('');
  }

  TestRunner.addResult('Initial:');
  await dumpRecords();

  TestRunner.addResult(`Filtered by 'AAA':`);
  view.textFilterUI.setValue('AAA', true);
  await dumpRecords();

  TestRunner.addResult(`Filtered by 'BBB':`);
  view.textFilterUI.setValue('BBB', true);
  await dumpRecords();

  TestRunner.completeTest();
})();
