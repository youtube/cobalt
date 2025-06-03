// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as Timeline from 'devtools/panels/timeline/timeline.js';

(async function() {

  await TestRunner.showPanel('timeline');
  TestRunner.addResult('Performance panel loaded.');

  const tabbedPane = Timeline.TimelinePanel.TimelinePanel.instance().flameChart.detailsView.tabbedPane;
  tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.EventLog);

  TestRunner.addResult('Loading a performance model.');
  const view = tabbedPane.visibleView;
  const model = await PerformanceTestRunner.createPerformanceModelWithEvents([{}]);
  view.setModel(model, PerformanceTestRunner.mainTrack());
  view.updateContents(Timeline.TimelineSelection.TimelineSelection.fromRange(
    model.timelineModel().minimumRecordTime(), model.timelineModel().maximumRecordTime()));

  TestRunner.addResult('Running aXe on the event log pane.');
  await AxeCoreTestRunner.runValidation(view.contentElement);
  TestRunner.completeTest();
})();
