// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCED_TASK_RUNNER_H_
#define BASE_TASK_SEQUENCED_TASK_RUNNER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/task/delay_policy.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/task_runner.h"
#include "base/types/pass_key.h"

namespace blink {
class LowPrecisionTimer;
class TimerBase;
class TimerBasedTickProvider;
class WebRtcTaskQueue;
}
namespace webrtc {
class ThreadWrapper;
}  // namespace webrtc
namespace media {
class AlsaPcmOutputStream;
class AlsaPcmInputStream;
class FakeAudioWorker;
}  // namespace media

namespace base {

namespace internal {
class DelayTimerBase;
class DelayedTaskManager;
}
class DeadlineTimer;
class MetronomeTimer;
class TimeDelta;
class TimeTicks;

namespace subtle {

// Used to restrict access to PostCancelableDelayedTaskAt() to authorize
// callers.
class PostDelayedTaskPassKey {
 private:
  // Avoid =default to disallow creation by uniform initialization.
  PostDelayedTaskPassKey() {}

  friend class base::internal::DelayTimerBase;
  friend class base::internal::DelayedTaskManager;
  friend class base::DeadlineTimer;
  friend class base::MetronomeTimer;
  friend class blink::LowPrecisionTimer;
  friend class blink::TimerBase;
  friend class blink::TimerBasedTickProvider;
  friend class blink::WebRtcTaskQueue;
  friend class PostDelayedTaskPassKeyForTesting;
  friend class webrtc::ThreadWrapper;
  friend class media::AlsaPcmOutputStream;
  friend class media::AlsaPcmInputStream;
  friend class media::FakeAudioWorker;
};

class PostDelayedTaskPassKeyForTesting : public PostDelayedTaskPassKey {};

}  // namespace subtle

// A SequencedTaskRunner is a subclass of TaskRunner that provides
// additional guarantees on the order that tasks are started, as well
// as guarantees on when tasks are in sequence, i.e. one task finishes
// before the other one starts.
//
// Summary
// -------
// Non-nested tasks with the same delay will run one by one in FIFO
// order.
//
// Detailed guarantees
// -------------------
//
// SequencedTaskRunner also adds additional methods for posting
// non-nestable tasks.  In general, an implementation of TaskRunner
// may expose task-running methods which are themselves callable from
// within tasks.  A non-nestable task is one that is guaranteed to not
// be run from within an already-running task.  Conversely, a nestable
// task (the default) is a task that can be run from within an
// already-running task.
//
// The guarantees of SequencedTaskRunner are as follows:
//
//   - Given two tasks T2 and T1, T2 will start after T1 starts if:
//
//       * T2 is posted after T1; and
//       * T2 has equal or higher delay than T1; and
//       * T2 is non-nestable or T1 is nestable.
//
//   - If T2 will start after T1 starts by the above guarantee, then
//     T2 will start after T1 finishes and is destroyed if:
//
//       * T2 is non-nestable, or
//       * T1 doesn't call any task-running methods.
//
//   - If T2 will start after T1 finishes by the above guarantee, then
//     all memory changes in T1 and T1's destruction will be visible
//     to T2.
//
//   - If T2 runs nested within T1 via a call to the task-running
//     method M, then all memory changes in T1 up to the call to M
//     will be visible to T2, and all memory changes in T2 will be
//     visible to T1 from the return from M.
//
// Note that SequencedTaskRunner does not guarantee that tasks are run
// on a single dedicated thread, although the above guarantees provide
// most (but not all) of the same guarantees.  If you do need to
// guarantee that tasks are run on a single dedicated thread, see
// SingleThreadTaskRunner (in single_thread_task_runner.h).
//
// Some corollaries to the above guarantees, assuming the tasks in
// question don't call any task-running methods:
//
//   - Tasks posted via PostTask are run in FIFO order.
//
//   - Tasks posted via PostNonNestableTask are run in FIFO order.
//
//   - Tasks posted with the same delay and the same nestable state
//     are run in FIFO order.
//
//   - A list of tasks with the same nestable state posted in order of
//     non-decreasing delay is run in FIFO order.
//
//   - A list of tasks posted in order of non-decreasing delay with at
//     most a single change in nestable state from nestable to
//     non-nestable is run in FIFO order. (This is equivalent to the
//     statement of the first guarantee above.)
//
// Some theoretical implementations of SequencedTaskRunner:
//
//   - A SequencedTaskRunner that wraps a regular TaskRunner but makes
//     sure that only one task at a time is posted to the TaskRunner,
//     with appropriate memory barriers in between tasks.
//
//   - A SequencedTaskRunner that, for each task, spawns a joinable
//     thread to run that task and immediately quit, and then
//     immediately joins that thread.
//
//   - A SequencedTaskRunner that stores the list of posted tasks and
//     has a method Run() that runs each runnable task in FIFO order
//     that can be called from any thread, but only if another
//     (non-nested) Run() call isn't already happening.
//
// SequencedTaskRunner::GetCurrentDefault() can be used while running
// a task to retrieve the default SequencedTaskRunner for the current
// sequence.
class BASE_EXPORT SequencedTaskRunner : public TaskRunner {
 public:
  // The two PostNonNestable*Task methods below are like their
  // nestable equivalents in TaskRunner, but they guarantee that the
  // posted task will not run nested within an already-running task.
  //
  // A simple corollary is that posting a task as non-nestable can
  // only delay when the task gets run.  That is, posting a task as
  // non-nestable may not affect when the task gets run, or it could
  // make it run later than it normally would, but it won't make it
  // run earlier than it normally would.

  // TODO(akalin): Get rid of the boolean return value for the methods
  // below.

  bool PostNonNestableTask(const Location& from_here, OnceClosure task);

  virtual bool PostNonNestableDelayedTask(const Location& from_here,
                                          OnceClosure task,
                                          base::TimeDelta delay) = 0;

  // Posts the given |task| to be run only after |delay| has passed. Returns a
  // handle that can be used to cancel the task. This should not be used
  // directly. Consider using higher level timer primitives in
  // base/timer/timer.h.
  //
  // The handle is only guaranteed valid while the task is pending execution.
  // This means that it may be invalid if the posting failed, and will be
  // invalid while the task is executing. Calling CancelTask() on an invalid
  // handle is a no-op.
  //
  // This method and the handle it returns are not thread-safe and can only be
  // used from the sequence this task runner runs its tasks on.
  virtual DelayedTaskHandle PostCancelableDelayedTask(
      subtle::PostDelayedTaskPassKey,
      const Location& from_here,
      OnceClosure task,
      TimeDelta delay);

  // Posts the given |task| to be run at |delayed_run_time| (or immediately if
  // in the past), following |delay_policy|. Returns a handle that can be used
  // to cancel the task. This should not be used directly. Consider using higher
  // level timer primitives in base/timer/timer.h.
  [[nodiscard]] virtual DelayedTaskHandle PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKey,
      const Location& from_here,
      OnceClosure task,
      TimeTicks delayed_run_time,
      subtle::DelayPolicy delay_policy);

  // Posts the given |task| to be run at |delayed_run_time| (or immediately if
  // in the past), following |delay_policy|. This is used by the default
  // implementation of PostCancelableDelayedTaskAt(). The default behavior
  // subtracts TimeTicks::Now() from |delayed_run_time| to get a delay. See
  // base::Timer to post precise/repeating timeouts.
  // TODO(1153139): Make pure virtual once all SequencedTaskRunners implement
  // this.
  virtual bool PostDelayedTaskAt(subtle::PostDelayedTaskPassKey,
                                 const Location& from_here,
                                 OnceClosure task,
                                 TimeTicks delayed_run_time,
                                 subtle::DelayPolicy delay_policy);

  // Submits a non-nestable task to delete the given object.  Returns
  // true if the object may be deleted at some point in the future,
  // and false if the object definitely will not be deleted.
  //
  // By default, this leaks `object` if the deleter task doesn't run, e.g. if
  // the underlying task queue is shut down first. Subclasses can override this
  // behavior by specializing `DeleteOrReleaseSoonInternal()`.
  template <class T>
  bool DeleteSoon(const Location& from_here, const T* object) {
    return DeleteOrReleaseSoonInternal(from_here, &DeleteHelper<T>::DoDelete,
                                       object);
  }

  template <class T>
  bool DeleteSoon(const Location& from_here, std::unique_ptr<T> object) {
    return DeleteOrReleaseSoonInternal(
        from_here, &DeleteUniquePtrHelper<T>::DoDelete, object.release());
  }

  // Submits a non-nestable task to release the given object.
  //
  // By default, this leaks `object` if the releaser task doesn't run, e.g. if
  // the underlying task queue is shut down first. Subclasses can override this
  // behavior by specializing `DeleteOrReleaseSoonInternal()`.
  //
  // ReleaseSoon makes sure that the object it the scoped_refptr points to gets
  // properly released on the correct thread.
  // We apply ReleaseSoon to the rvalue as the side-effects can be unclear to
  // the caller if an lvalue is used. That being so, the scoped_refptr should
  // always be std::move'd.
  // Example use:
  //
  // scoped_refptr<T> foo_scoped_refptr;
  // ...
  // task_runner->ReleaseSoon(std::move(foo_scoped_refptr));
  template <class T>
  void ReleaseSoon(const Location& from_here, scoped_refptr<T>&& object) {
    if (!object)
      return;

    DeleteOrReleaseSoonInternal(from_here, &ReleaseHelper<T>::DoRelease,
                                object.release());
  }

  // Returns true iff tasks posted to this TaskRunner are sequenced
  // with this call.
  //
  // In particular:
  // - Returns true if this is a SequencedTaskRunner to which the
  //   current task was posted.
  // - Returns true if this is a SequencedTaskRunner bound to the
  //   same sequence as the SequencedTaskRunner to which the current
  //   task was posted.
  // - Returns true if this is a SingleThreadTaskRunner bound to
  //   the current thread.
  virtual bool RunsTasksInCurrentSequence() const = 0;

  // Returns the default SequencedThreadTaskRunner for the current task. It
  // should only be called if HasCurrentDefault() returns true (see the comment
  // there for the requirements).
  //
  // It is "default" in the sense that if the current sequence multiplexes
  // multiple task queues (e.g. BrowserThread::UI), this will return the default
  // task queue. A caller that wants a specific task queue should obtain it
  // directly instead of going through this API.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md#Posting-to-the-Current-Virtual_Thread
  // for details
  [[nodiscard]] static const scoped_refptr<SequencedTaskRunner>&
  GetCurrentDefault();

  // Returns true if one of the following conditions is fulfilled:
  // a) A SequencedTaskRunner has been assigned to the current thread by
  //    instantiating a SequencedTaskRunner::CurrentDefaultHandle.
  // b) The current thread has a SingleThreadTaskRunner::CurrentDefaultHandle
  //    (which includes any thread that runs a MessagePump).
  [[nodiscard]] static bool HasCurrentDefault();

  class BASE_EXPORT CurrentDefaultHandle {
   public:
    // Binds `task_runner` to the current thread so that it is returned by
    // GetCurrentDefault() in the scope of the constructed
    // `CurrentDefaultHandle`.
    explicit CurrentDefaultHandle(
        scoped_refptr<SequencedTaskRunner> task_runner);

    CurrentDefaultHandle(const CurrentDefaultHandle&) = delete;
    CurrentDefaultHandle& operator=(const CurrentDefaultHandle&) = delete;

    ~CurrentDefaultHandle();

   private:
    friend class SequencedTaskRunner;
    friend class CurrentHandleOverride;

    const AutoReset<CurrentDefaultHandle*> resetter_;

    scoped_refptr<SequencedTaskRunner> task_runner_;
  };

 protected:
  ~SequencedTaskRunner() override = default;

  // Helper to allow SingleThreadTaskRunner::CurrentDefaultHandle to double as a
  // SequencedTaskRunner::CurrentDefaultHandle.
  static void SetCurrentDefaultHandleTaskRunner(
      CurrentDefaultHandle& current_default,
      scoped_refptr<SequencedTaskRunner> task_runner) {
    current_default.task_runner_ = task_runner;
  }

  virtual bool DeleteOrReleaseSoonInternal(const Location& from_here,
                                           void (*deleter)(const void*),
                                           const void* object);
};

// Sample usage with std::unique_ptr :
// std::unique_ptr<Foo, base::OnTaskRunnerDeleter> ptr(
//     new Foo, base::OnTaskRunnerDeleter(my_task_runner));
//
// For RefCounted see base::RefCountedDeleteOnSequence.
struct BASE_EXPORT OnTaskRunnerDeleter {
  explicit OnTaskRunnerDeleter(scoped_refptr<SequencedTaskRunner> task_runner);
  ~OnTaskRunnerDeleter();

  OnTaskRunnerDeleter(OnTaskRunnerDeleter&&);
  OnTaskRunnerDeleter& operator=(OnTaskRunnerDeleter&&);

  // For compatibility with std:: deleters.
  template <typename T>
  void operator()(const T* ptr) {
    if (ptr)
      task_runner_->DeleteSoon(FROM_HERE, ptr);
  }

  scoped_refptr<SequencedTaskRunner> task_runner_;
};

}  // namespace base

#endif  // BASE_TASK_SEQUENCED_TASK_RUNNER_H_
