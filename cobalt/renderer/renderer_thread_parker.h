// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDERER_RENDERER_THREAD_PARKER_H_
#define COBALT_RENDERER_RENDERER_THREAD_PARKER_H_

#include <atomic>

#include "base/synchronization/waitable_event.h"
#include "base/task/task_observer.h"
#include "base/time/time.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace cobalt {

// Stops the in-process renderer main thread from running any more work once
// application teardown starts.
//
// In single-process mode the renderer main thread is not joined at shutdown
// (see ~RenderProcessHostImpl). It therefore outlives objects that are
// destroyed on the browser main thread during kSbEventTypeStop, such as the
// ContentRendererClient, and any task or script it runs afterwards can use
// freed memory. On platforms where the process does not exit immediately after
// kSbEventTypeStop, that results in a crash.
//
// A parker observes the renderer main thread. Once parking is requested from
// another thread, the renderer main thread blocks at the next point where it
// would start a task, or at the next V8 interrupt check if it is in the middle
// of running script. It is not resumed, so parking must only be requested when
// the process is about to exit.
class RendererThreadParker : public base::TaskObserver {
 public:
  RendererThreadParker();
  RendererThreadParker(const RendererThreadParker&) = delete;
  RendererThreadParker& operator=(const RendererThreadParker&) = delete;
  ~RendererThreadParker() override;

  // Returns the process-wide instance, which is never destroyed.
  static RendererThreadParker* GetInstance();

  // Starts observing the calling thread. Must be called on the thread that is
  // to be parked, and at most once.
  void InstallOnCurrentThread();

  // Requests that the observed thread stop running work. Does not block. May
  // be called from any other thread. Returns false if the parker is not
  // installed.
  //
  // If the observed thread is running script, the script is interrupted. If
  // it is blocked in native code, for example in a synchronous IPC, it parks
  // once that code returns to the task runner or to script.
  bool RequestPark();

  // Waits up to |timeout| for the observed thread to comply with an earlier
  // RequestPark(). Returns true if the thread is parked, or is idle and will
  // park before it runs anything else. Returns false if it was still busy when
  // |timeout| expired; the request stays in effect regardless.
  bool WaitUntilQuiescent(base::TimeDelta timeout);

  // Whether the observed thread is currently blocked by this parker.
  bool is_parked() const { return parked_.load(); }

  // Withdraws the park request and resumes the observed thread if it is
  // parked.
  void UnparkForTesting();

  // base::TaskObserver implementation.
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

 private:
  static void OnV8Interrupt(v8::Isolate* isolate, void* data);

  // Blocks the calling thread until UnparkForTesting() is called.
  void Park();

  std::atomic<bool> installed_{false};
  std::atomic<bool> park_requested_{false};
  std::atomic<bool> parked_{false};
  // Number of tasks currently on the observed thread's stack. More than one
  // when a nested run loop is running.
  std::atomic<int> task_depth_{0};
  std::atomic<v8::Isolate*> isolate_{nullptr};
  // Signaled by the observed thread when, after a park request, it has parked
  // or has no task on its stack.
  base::WaitableEvent quiescent_;
  // What a parked thread blocks on. Only ever signaled in tests.
  base::WaitableEvent unpark_;
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_RENDERER_THREAD_PARKER_H_
