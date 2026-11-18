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

#include "cobalt/renderer/renderer_thread_parker.h"

#include <atomic>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_observer.h"
#include "base/thread_annotations.h"
#include "v8/include/v8-isolate.h"

namespace cobalt {

namespace {

// Never destroyed, so a thread that parks late, after teardown has given up
// waiting for it, does not touch freed state.
struct RendererThreadParkState {
  RendererThreadParkState() {
    // A parked thread never wakes; keep its wait out of blocking-call tracking.
    unpark_event_for_testing.declare_only_used_while_idle();
  }

  base::Lock lock;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner GUARDED_BY(lock);
  std::atomic<bool> park_requested{false};
  std::atomic<v8::Isolate*> v8_isolate{nullptr};
  base::WaitableEvent parked_event{base::WaitableEvent::ResetPolicy::MANUAL};
  // Only signaled in tests.
  base::WaitableEvent unpark_event_for_testing{
      base::WaitableEvent::ResetPolicy::MANUAL};
};

RendererThreadParkState& GetRendererThreadParkState() {
  static base::NoDestructor<RendererThreadParkState> park_state;
  return *park_state;
}

void Park() {
  LOG(INFO) << "Renderer main thread parked for shutdown.";
  GetRendererThreadParkState().parked_event.Signal();
  GetRendererThreadParkState().unpark_event_for_testing.Wait();
}

// Parks the thread before its next task, so that work already queued when
// parking is requested does not run.
class ParkObserver : public base::TaskObserver {
 public:
  void WillProcessTask(const base::PendingTask&, bool) override {
    if (GetRendererThreadParkState().park_requested.load()) {
      Park();
    }
  }
  void DidProcessTask(const base::PendingTask&) override {}
};

}  // namespace

void RegisterCurrentRendererThreadForParking() {
  static base::NoDestructor<ParkObserver> observer;
  RendererThreadParkState& park_state = GetRendererThreadParkState();
  park_state.v8_isolate.store(v8::Isolate::TryGetCurrent());
  base::CurrentThread::Get()->AddTaskObserver(observer.get());
  base::AutoLock auto_lock(park_state.lock);
  park_state.task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void RequestRendererThreadPark() {
  RendererThreadParkState& park_state = GetRendererThreadParkState();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  {
    base::AutoLock auto_lock(park_state.lock);
    task_runner = park_state.task_runner;
  }
  if (!task_runner) {
    return;
  }
  park_state.park_requested.store(true);
  // An idle thread needs a task to reach the observer.
  task_runner->PostTask(FROM_HERE, base::DoNothing());
  // Running script may not return to the task loop in time, so interrupt it.
  if (v8::Isolate* v8_isolate = park_state.v8_isolate.load()) {
    v8_isolate->RequestInterrupt([](v8::Isolate*, void*) { Park(); }, nullptr);
  }
}

bool WaitUntilRendererThreadParked(base::TimeDelta timeout) {
  RendererThreadParkState& park_state = GetRendererThreadParkState();
  {
    base::AutoLock auto_lock(park_state.lock);
    if (!park_state.task_runner) {
      return true;
    }
  }
  return park_state.parked_event.TimedWait(timeout);
}

void RegisterCurrentRendererThreadForParkingForTesting() {
  GetRendererThreadParkState().parked_event.Reset();
  GetRendererThreadParkState().unpark_event_for_testing.Reset();
  RegisterCurrentRendererThreadForParking();
}

void UnparkRendererThreadForTesting() {
  RendererThreadParkState& park_state = GetRendererThreadParkState();
  {
    base::AutoLock auto_lock(park_state.lock);
    park_state.task_runner = nullptr;
  }
  park_state.v8_isolate.store(nullptr);
  park_state.park_requested.store(false);
  park_state.unpark_event_for_testing.Signal();
}

}  // namespace cobalt
