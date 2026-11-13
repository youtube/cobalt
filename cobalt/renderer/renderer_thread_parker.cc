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

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "v8/include/v8-isolate.h"

namespace cobalt {

RendererThreadParker::RendererThreadParker()
    : quiescent_(base::WaitableEvent::ResetPolicy::MANUAL,
                 base::WaitableEvent::InitialState::NOT_SIGNALED),
      unpark_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  // A parked thread stays blocked until the process exits. Keep that wait out
  // of blocking-call tracking, which relies on state that teardown destroys.
  unpark_.declare_only_used_while_idle();
}

RendererThreadParker::~RendererThreadParker() = default;

// static
RendererThreadParker* RendererThreadParker::GetInstance() {
  static base::NoDestructor<RendererThreadParker> instance;
  return instance.get();
}

void RendererThreadParker::InstallOnCurrentThread() {
  CHECK(!installed_.exchange(true));
  isolate_.store(v8::Isolate::TryGetCurrent());
  base::CurrentThread::Get()->AddTaskObserver(this);
}

bool RendererThreadParker::RequestPark() {
  if (!installed_.load()) {
    return false;
  }
  park_requested_.store(true);
  // If the thread is running script, interrupt it rather than waiting for the
  // script to return, which may never happen.
  if (v8::Isolate* isolate = isolate_.load()) {
    isolate->RequestInterrupt(&RendererThreadParker::OnV8Interrupt, this);
  }
  return true;
}

bool RendererThreadParker::WaitUntilQuiescent(base::TimeDelta timeout) {
  CHECK(park_requested_.load());
  // The order matters: the request was published before |task_depth_| is read
  // here, and the observed thread publishes |task_depth_| before it reads the
  // request. So if no task is seen here, the next task to start sees the
  // request and parks.
  if (task_depth_.load() == 0) {
    return true;
  }
  return quiescent_.TimedWait(timeout);
}

void RendererThreadParker::UnparkForTesting() {
  park_requested_.store(false);
  unpark_.Signal();
}

void RendererThreadParker::WillProcessTask(const base::PendingTask&, bool) {
  if (!isolate_.load()) {
    isolate_.store(v8::Isolate::TryGetCurrent());
  }
  task_depth_.fetch_add(1);
  if (park_requested_.load()) {
    Park();
  }
}

void RendererThreadParker::DidProcessTask(const base::PendingTask&) {
  if (task_depth_.load() == 0) {
    // The end of the task that installed this observer, whose start was not
    // observed.
    return;
  }
  if (task_depth_.fetch_sub(1) == 1 && park_requested_.load()) {
    // Nothing is running on this thread any more, and the next task parks
    // before it starts.
    quiescent_.Signal();
  }
}

// static
void RendererThreadParker::OnV8Interrupt(v8::Isolate*, void* data) {
  auto* parker = static_cast<RendererThreadParker*>(data);
  if (parker->park_requested_.load()) {
    parker->Park();
  }
}

void RendererThreadParker::Park() {
  LOG(INFO) << "Renderer main thread parked for shutdown.";
  parked_.store(true);
  quiescent_.Signal();
  unpark_.Wait();
  parked_.store(false);
}

}  // namespace cobalt
