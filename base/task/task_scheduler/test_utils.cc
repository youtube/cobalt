// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/task_scheduler/scheduler_worker_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace test {

MockSchedulerWorkerObserver::MockSchedulerWorkerObserver()
    : on_main_exit_cv_(lock_.CreateConditionVariable()) {}

MockSchedulerWorkerObserver::~MockSchedulerWorkerObserver() {
  WaitCallsOnMainExit();
}

void MockSchedulerWorkerObserver::AllowCallsOnMainExit(int num_calls) {
  AutoSchedulerLock auto_lock(lock_);
  EXPECT_EQ(0, allowed_calls_on_main_exit_);
  allowed_calls_on_main_exit_ = num_calls;
}

void MockSchedulerWorkerObserver::WaitCallsOnMainExit() {
  AutoSchedulerLock auto_lock(lock_);
  while (allowed_calls_on_main_exit_ != 0)
    on_main_exit_cv_->Wait();
}

void MockSchedulerWorkerObserver::OnSchedulerWorkerMainExit() {
  AutoSchedulerLock auto_lock(lock_);
  EXPECT_GE(allowed_calls_on_main_exit_, 0);
  --allowed_calls_on_main_exit_;
  if (allowed_calls_on_main_exit_ == 0)
    on_main_exit_cv_->Signal();
}

scoped_refptr<Sequence> CreateSequenceWithTask(Task task,
                                               const TaskTraits& traits) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(traits);
  sequence->PushTask(std::move(task));
  return sequence;
}

scoped_refptr<TaskRunner> CreateTaskRunnerWithExecutionMode(
    SchedulerWorkerPool* worker_pool,
    test::ExecutionMode execution_mode) {
  // Allow tasks posted to the returned TaskRunner to wait on a WaitableEvent.
  const TaskTraits traits = {WithBaseSyncPrimitives()};
  switch (execution_mode) {
    case test::ExecutionMode::PARALLEL:
      return worker_pool->CreateTaskRunnerWithTraits(traits);
    case test::ExecutionMode::SEQUENCED:
      return worker_pool->CreateSequencedTaskRunnerWithTraits(traits);
    default:
      // Fall through.
      break;
  }
  ADD_FAILURE() << "Unexpected ExecutionMode";
  return nullptr;
}

}  // namespace test
}  // namespace internal
}  // namespace base
