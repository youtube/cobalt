// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SINGLE_THREAD_TASK_RUNNER_H_
#define BASE_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/base_export.h"
#include "base/bind.h"
#include "base/sequenced_task_runner.h"

namespace base {

// A SingleThreadTaskRunner is a SequencedTaskRunner with one more
// guarantee; namely, that all tasks are run on a single dedicated
// thread.  Most use cases require only a SequencedTaskRunner, unless
// there is a specific need to run tasks on only a single thread.
//
// SingleThreadTaskRunner implementations might:
//   - Post tasks to an existing thread's MessageLoop (see
//     MessageLoop::task_runner()).
//   - Create their own worker thread and MessageLoop to post tasks to.
//   - Add tasks to a FIFO and signal to a non-MessageLoop thread for them to
//     be processed. This allows TaskRunner-oriented code run on threads
//     running other kinds of message loop, e.g. Jingle threads.
class BASE_EXPORT SingleThreadTaskRunner : public SequencedTaskRunner {
 public:
  // A more explicit alias to RunsTasksInCurrentSequence().
  bool BelongsToCurrentThread() const { return RunsTasksInCurrentSequence(); }

#if defined(STARBOARD)
  // Like PostTask, but blocks until the posted task completes. Returns false
  // and does not block if task was not posted.
  virtual void PostBlockingTask(const base::Location& from_here,
                                const Closure& task);

  // Adds a fence at the end of this MessageLoop's task queue, and then blocks
  // until it has been reached. It is forbidden to call this method from the
  // thread of the MessageLoop being posted to. One should exercise extreme
  // caution when using this, as blocking between MessageLoops can cause
  // deadlocks and is contraindicated in the Actor model of multiprogramming.
  void WaitForFence() {
    struct Fence {
      static void Task() {}
    };
    PostBlockingTask(FROM_HERE, base::Bind(&Fence::Task));
  }
#endif

 protected:
  ~SingleThreadTaskRunner() override = default;
};

}  // namespace base

#endif  // BASE_SINGLE_THREAD_TASK_RUNNER_H_
