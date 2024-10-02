// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace unexportable_keys {

// Interface for tasks scheduled on `BackgroundLongTaskScheduler`.
class BackgroundTask {
 public:
  // Different statuses that the task can have.
  enum class Status {
    kPending,   // The task is waiting in a queue.
    kCanceled,  // The task has been canceled by the caller.
    kPosted     // The task has been posted on the background thread.
  };

  virtual ~BackgroundTask() = default;

  // Runs the task on `background_task_runner` and invokes
  // `on_complete_callback` with `this` on the posting thread once the task
  // completes.
  virtual void Run(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      base::OnceCallback<void(BackgroundTask* task)> on_complete_callback) = 0;

  // Returns the current status of the task.
  virtual Status GetStatus() const = 0;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_
