// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_LIBDISPATCH_SEQUENCED_TASK_RUNNER_H_
#define BASE_MAC_LIBDISPATCH_SEQUENCED_TASK_RUNNER_H_

#include <dispatch/dispatch.h>

#include "base/single_thread_task_runner.h"

namespace base {
namespace mac {

// This is an implementation of the TaskRunner interface that runs closures on
// a thread managed by Apple's libdispatch. This has the benefit of being able
// to PostTask() and friends to a dispatch queue, while being reusable as a
// dispatch_queue_t.
//
// One would use this class if an object lives exclusively on one thread but
// needs a dispatch_queue_t for use in a system API. This ensures all dispatch
// callbacks happen on the same thread as Closure tasks.
//
// A LibDispatchTaskRunner will continue to run until all references to the
// underlying dispatch queue are released.
//
// Important Notes:
//   - There is no MessageLoop running on this thread, and ::current() returns
//     NULL.
//   - No nested loops can be run, and all tasks are run non-nested.
//   - Work scheduled via libdispatch runs at the same priority as and is
//     interleaved with posted tasks, though FIFO order is guaranteed.
//
class BASE_EXPORT LibDispatchTaskRunner : public base::SingleThreadTaskRunner {
 public:
  // Starts a new serial dispatch queue with a given name.
  explicit LibDispatchTaskRunner(const char* name);

  // base::TaskRunner:
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

  // base::SequencedTaskRunner:
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const Closure& task,
      base::TimeDelta delay) OVERRIDE;

  // Returns the dispatch queue associated with this task runner, for use with
  // system APIs that take dispatch queues. The caller is responsible for
  // retaining the result.
  dispatch_queue_t GetDispatchQueue() const;

 protected:
  virtual ~LibDispatchTaskRunner();

 private:
  dispatch_queue_t queue_;
};

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_LIBDISPATCH_SEQUENCED_TASK_RUNNER_H_
