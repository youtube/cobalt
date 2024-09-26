// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_

#include <memory>
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace v8 {
class Isolate;
}

namespace base {
class TaskObserver;
}

namespace blink {

class CompositorThreadScheduler;
class MainThreadScheduler;

// This class is used to submit tasks and pass other information from Blink to
// the platform's scheduler.
class PLATFORM_EXPORT ThreadScheduler {
 public:
  // Return the current thread's ThreadScheduler.
  static ThreadScheduler* Current();

  // Returns compositor thread scheduler for the compositor thread
  // of the current process.
  static blink::CompositorThreadScheduler* CompositorThreadScheduler();

  virtual ~ThreadScheduler() = default;

  // Called to prevent any more pending tasks from running. Must be called on
  // the associated WebThread.
  virtual void Shutdown() = 0;

  // Returns true if there is high priority work pending on the associated
  // WebThread and the caller should yield to let the scheduler service that
  // work.  Must be called on the associated WebThread.
  virtual bool ShouldYieldForHighPriorityWork() = 0;

  // Schedule an idle task to run the associated WebThread. For non-critical
  // tasks which may be reordered relative to other task types and may be
  // starved for an arbitrarily long time if no idle time is available.
  // Takes ownership of |IdleTask|. Can be called from any thread.
  virtual void PostIdleTask(const base::Location&, Thread::IdleTask) = 0;

  // As above, except that the task is guaranteed to not run before |delay|.
  // Takes ownership of |IdleTask|. Can be called from any thread.
  virtual void PostDelayedIdleTask(const base::Location&,
                                   base::TimeDelta delay,
                                   Thread::IdleTask) = 0;

  // Like postIdleTask but guarantees that the posted task will not run
  // nested within an already-running task. Posting an idle task as
  // non-nestable may not affect when the task gets run, or it could
  // make it run later than it normally would, but it won't make it
  // run earlier than it normally would.
  virtual void PostNonNestableIdleTask(const base::Location&,
                                       Thread::IdleTask) = 0;

  // Returns a task runner for kV8 tasks. Can be called from any thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() = 0;

  // Returns a task runner for tasks to deallocate objects on the appropriate
  // thread. This runner should only be used for freeing of resources.
  // Execution of javascript will be prevented in the future on this task
  // runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() = 0;

  // Returns the current time recognized by the scheduler, which may perhaps
  // be based on a real or virtual time domain. Used by Timer.
  virtual base::TimeTicks MonotonicallyIncreasingVirtualTime() = 0;

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this scheduler was created on.
  virtual void AddTaskObserver(base::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(base::TaskObserver* task_observer) = 0;

  // Associates |isolate| to the scheduler.
  virtual void SetV8Isolate(v8::Isolate* isolate) = 0;

  // Returns the scheduler's TaskAttributionTracker is we're running on the main
  // thread, or a nullptr otherwise.
  virtual scheduler::TaskAttributionTracker* GetTaskAttributionTracker() {
    return nullptr;
  }

  // Convert this into a MainThreadScheduler if it is one.
  virtual MainThreadScheduler* ToMainThreadScheduler() { return nullptr; }

  // Test helpers.

  virtual void InitializeTaskAttributionTracker(
      std::unique_ptr<scheduler::TaskAttributionTracker>) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_
