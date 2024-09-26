// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests sorting in Summary view of detailed heap snapshots.\n`);
  await TestRunner.loadTestModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapProfilerTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testSorting(next) {
    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapProfilerTestRunner.switchToView('Summary', step2);
    }

    var columns;
    var currentColumn;
    var currentColumnOrder;

    function step2() {
      columns = HeapProfilerTestRunner.viewColumns();
      currentColumn = 0;
      currentColumnOrder = false;
      setTimeout(step3, 0);
    }

    function step3() {
      if (currentColumn >= columns.length) {
        setTimeout(next, 0);
        return;
      }

      HeapProfilerTestRunner.clickColumn(columns[currentColumn], step4);
    }

    function step4(newColumnState) {
      columns[currentColumn] = newColumnState;
      var contents = HeapProfilerTestRunner.columnContents(columns[currentColumn]);
      TestRunner.assertEquals(true, !!contents.length, 'column contents');
      var sortTypes = {object: 'text', distance: 'number', count: 'number', shallowSize: 'size', retainedSize: 'size'};
      TestRunner.assertEquals(true, !!sortTypes[columns[currentColumn].id], 'sort by id');
      HeapProfilerTestRunner.checkArrayIsSorted(
          contents, sortTypes[columns[currentColumn].id], columns[currentColumn].sort);

      if (!currentColumnOrder)
        currentColumnOrder = true;
      else {
        currentColumnOrder = false;
        ++currentColumn;
      }
      setTimeout(step3, 0);
    }
  }]);
})();
