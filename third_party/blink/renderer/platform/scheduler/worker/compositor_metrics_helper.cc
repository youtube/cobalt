// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_metrics_helper.h"

namespace blink {
namespace scheduler {

CompositorMetricsHelper::CompositorMetricsHelper(
    bool has_cpu_timing_for_each_task)
    : MetricsHelper(ThreadType::kCompositorThread,
                    has_cpu_timing_for_each_task) {}

CompositorMetricsHelper::~CompositorMetricsHelper() {}

void CompositorMetricsHelper::RecordTaskMetrics(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(task, task_timing))
    return;

  MetricsHelper::RecordCommonTaskMetrics(task, task_timing);
}

}  // namespace scheduler
}  // namespace blink
