// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_runner_test_template.h"

namespace base {

namespace internal {

TaskTracker::TaskTracker() {}

TaskTracker::~TaskTracker() {}

Closure TaskTracker::WrapTask(const Closure& task, int i) {
  return Bind(&TaskTracker::RunTask, this, task, i);
}

void TaskTracker::RunTask(const Closure& task, int i) {
  AutoLock lock(task_run_counts_lock_);
  if (!task.is_null()) {
    task.Run();
  }
  ++task_run_counts_[i];
}

std::map<int, int> TaskTracker::GetTaskRunCounts() const {
  AutoLock lock(task_run_counts_lock_);
  return task_run_counts_;
}

void ExpectRunsTasksOnCurrentThread(
    bool expected_value,
    const scoped_refptr<TaskRunner>& task_runner) {
  EXPECT_EQ(expected_value, task_runner->RunsTasksOnCurrentThread());
}

}  // namespace internal

}  // namespace base
