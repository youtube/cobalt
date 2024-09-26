// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_long_task_scheduler.h"

#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/unexportable_keys/background_task.h"
#include "components/unexportable_keys/background_task_priority.h"

namespace unexportable_keys {

BackgroundLongTaskScheduler::BackgroundLongTaskScheduler(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)) {
  DCHECK(background_task_runner_);
}

BackgroundLongTaskScheduler::~BackgroundLongTaskScheduler() = default;

void BackgroundLongTaskScheduler::PostTask(std::unique_ptr<BackgroundTask> task,
                                           BackgroundTaskPriority priority) {
  GetTaskQueueForPriority(priority).push_back(std::move(task));
  // If no task is running, schedule `task` immediately.
  if (!running_task_) {
    MaybeRunNextPendingTask();
  }
}

void BackgroundLongTaskScheduler::OnTaskCompleted(BackgroundTask* task) {
  DCHECK_EQ(running_task_.get(), task);
  running_task_.reset();
  MaybeRunNextPendingTask();
}

void BackgroundLongTaskScheduler::MaybeRunNextPendingTask() {
  DCHECK(!running_task_);

  running_task_ = TakeNextPendingTask();
  if (!running_task_) {
    // There is no more pending tasks. Nothing to do.
    return;
  }
  running_task_->Run(
      background_task_runner_,
      base::BindOnce(&BackgroundLongTaskScheduler::OnTaskCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

BackgroundLongTaskScheduler::TaskQueue&
BackgroundLongTaskScheduler::GetTaskQueueForPriority(
    BackgroundTaskPriority priority) {
  size_t index = static_cast<size_t>(priority);
  CHECK_LT(index, kNumTaskPriorities);
  return task_queue_by_priority_[index];
}

BackgroundLongTaskScheduler::TaskQueue*
BackgroundLongTaskScheduler::GetHighestPriorityNonEmptyTaskQueue() {
  // Highest priority has the highest value.
  for (int i = kNumTaskPriorities - 1; i >= 0; --i) {
    TaskQueue& queue = task_queue_by_priority_[i];
    if (!queue.empty()) {
      return &queue;
    }
  }
  return nullptr;
}

std::unique_ptr<BackgroundTask>
BackgroundLongTaskScheduler::TakeNextPendingTask() {
  std::unique_ptr<BackgroundTask> next_task;
  while (!next_task) {
    TaskQueue* next_queue = GetHighestPriorityNonEmptyTaskQueue();
    if (!next_queue) {
      return nullptr;
    }

    next_task = std::move(next_queue->front());
    next_queue->pop_front();
    if (next_task->GetStatus() == BackgroundTask::Status::kCanceled) {
      // Dismiss a canceled task and try the next one.
      next_task.reset();
    } else {
      DCHECK_EQ(next_task->GetStatus(), BackgroundTask::Status::kPending);
    }
  }
  return next_task;
}

}  // namespace unexportable_keys
