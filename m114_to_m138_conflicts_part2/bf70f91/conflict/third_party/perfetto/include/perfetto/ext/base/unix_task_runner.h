/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_EXT_BASE_UNIX_TASK_RUNNER_H_
#define INCLUDE_PERFETTO_EXT_BASE_UNIX_TASK_RUNNER_H_

#include "perfetto/base/build_config.h"
#include "perfetto/base/task_runner.h"
<<<<<<< HEAD
#include "perfetto/base/thread_annotations.h"
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/event_fd.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_checker.h"

#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <poll.h>
#endif

namespace perfetto {
namespace base {

// Runs a task runner on the current thread.
//
// Implementation note: we currently assume (and enforce in debug builds) that
// Run() is called from the thread that constructed the UnixTaskRunner. This is
// not strictly necessary, and we could instead track the thread that invokes
// Run(). However, a related property that *might* be important to enforce is
// that the destructor runs on the task-running thread. Otherwise, if there are
// still-pending tasks at the time of destruction, we would destroy those
// outside of the task thread (which might be unexpected to the caller). On the
// other hand, the std::function task interface discourages use of any
// resource-owning tasks (as the callable needs to be copyable), so this might
// not be important in practice.
//
// TODO(rsavitski): consider adding a thread-check in the destructor, after
// auditing existing usages.
// TODO(primiano): rename this to TaskRunnerImpl. The "Unix" part is misleading
// now as it supports also Windows.
class UnixTaskRunner : public TaskRunner {
 public:
  UnixTaskRunner();
  ~UnixTaskRunner() override;

  // Start executing tasks. Doesn't return until Quit() is called. Run() may be
  // called multiple times on the same task runner.
  void Run();
  void Quit();

  // Checks whether there are any pending immediate tasks to run. Note that
  // delayed tasks don't count even if they are due to run.
  bool IsIdleForTesting();

<<<<<<< HEAD
  // Pretends (for the purposes of running delayed tasks) that time advanced by
  // `ms`.
  void AdvanceTimeForTesting(uint32_t ms);

=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // TaskRunner implementation:
  void PostTask(std::function<void()>) override;
  void PostDelayedTask(std::function<void()>, uint32_t delay_ms) override;
  void AddFileDescriptorWatch(PlatformHandle, std::function<void()>) override;
  void RemoveFileDescriptorWatch(PlatformHandle) override;
  bool RunsTasksOnCurrentThread() const override;

  // Returns true if the task runner is quitting, or has quit and hasn't been
  // restarted since. Exposed primarily for ThreadTaskRunner, not necessary for
  // normal use of this class.
  bool QuitCalled();

 private:
  void WakeUp();
<<<<<<< HEAD
  void UpdateWatchTasksLocked() PERFETTO_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  int GetDelayMsToNextTaskLocked() const
      PERFETTO_EXCLUSIVE_LOCKS_REQUIRED(lock_);
=======
  void UpdateWatchTasksLocked();
  int GetDelayMsToNextTaskLocked() const;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  void RunImmediateAndDelayedTask();
  void PostFileDescriptorWatches(uint64_t windows_wait_result);
  void RunFileDescriptorWatch(PlatformHandle);

  ThreadChecker thread_checker_;
<<<<<<< HEAD
  std::atomic<PlatformThreadId> created_thread_id_ = GetThreadId();
=======
  PlatformThreadId created_thread_id_ = GetThreadId();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  EventFd event_;

// The array of fds/handles passed to poll(2) / WaitForMultipleObjects().
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  std::vector<PlatformHandle> poll_fds_;
#else
  std::vector<struct pollfd> poll_fds_;
#endif

<<<<<<< HEAD
  std::mutex lock_;

  std::deque<std::function<void()>> immediate_tasks_ PERFETTO_GUARDED_BY(lock_);
  std::multimap<TimeMillis, std::function<void()>> delayed_tasks_
      PERFETTO_GUARDED_BY(lock_);
  bool quit_ PERFETTO_GUARDED_BY(lock_) = false;
  TimeMillis advanced_time_for_testing_ PERFETTO_GUARDED_BY(lock_) =
      TimeMillis(0);
=======
  // --- Begin lock-protected members ---

  std::mutex lock_;

  std::deque<std::function<void()>> immediate_tasks_;
  std::multimap<TimeMillis, std::function<void()>> delayed_tasks_;
  bool quit_ = false;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  struct WatchTask {
    std::function<void()> callback;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
    // On UNIX systems we make the FD number negative in |poll_fds_| to avoid
    // polling it again until the queued task runs. On Windows we can't do that.
    // Instead we keep track of its state here.
    bool pending = false;
#else
    size_t poll_fd_index;  // Index into |poll_fds_|.
#endif
  };

<<<<<<< HEAD
  std::map<PlatformHandle, WatchTask> watch_tasks_ PERFETTO_GUARDED_BY(lock_);
  bool watch_tasks_changed_ PERFETTO_GUARDED_BY(lock_) = false;
=======
  std::map<PlatformHandle, WatchTask> watch_tasks_;
  bool watch_tasks_changed_ = false;

  // --- End lock-protected members ---
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_UNIX_TASK_RUNNER_H_
