// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_
#define BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_

#include "base/task/task_scheduler/delayed_task_manager.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/scheduler_worker_observer.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_traits.h"
#include "base/task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace internal {

class SchedulerWorkerPool;
struct Task;

namespace test {

class MockSchedulerWorkerObserver : public SchedulerWorkerObserver {
 public:
  MockSchedulerWorkerObserver();
  ~MockSchedulerWorkerObserver();

  void AllowCallsOnMainExit(int num_calls);
  void WaitCallsOnMainExit();

  // SchedulerWorkerObserver:
  MOCK_METHOD0(OnSchedulerWorkerMainEntry, void());
  // This doesn't use MOCK_METHOD0 because some tests need to wait for all calls
  // to happen, which isn't possible with gmock.
  void OnSchedulerWorkerMainExit() override;

 private:
  SchedulerLock lock_;
  std::unique_ptr<ConditionVariable> on_main_exit_cv_;
  int allowed_calls_on_main_exit_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockSchedulerWorkerObserver);
};

// An enumeration of possible task scheduler TaskRunner types. Used to
// parametrize relevant task_scheduler tests.
enum class ExecutionMode { PARALLEL, SEQUENCED, SINGLE_THREADED };

// Creates a Sequence with given |traits| and pushes |task| to it. Returns that
// Sequence.
scoped_refptr<Sequence> CreateSequenceWithTask(Task task,
                                               const TaskTraits& traits);

// Creates a TaskRunner that posts tasks to |worker_pool| with the
// |execution_mode| execution mode and the WithBaseSyncPrimitives() trait.
// Caveat: this does not support ExecutionMode::SINGLE_THREADED.
scoped_refptr<TaskRunner> CreateTaskRunnerWithExecutionMode(
    SchedulerWorkerPool* worker_pool,
    test::ExecutionMode execution_mode);

}  // namespace test
}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_
