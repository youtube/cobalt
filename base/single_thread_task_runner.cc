// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "base/single_thread_task_runner.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"

namespace base {
#if defined(STARBOARD)
namespace {

// Runs the given task, and then signals the given WaitableEvent.
void RunAndSignal(const base::Closure& task, base::WaitableEvent* event) {
  TRACE_EVENT0("task", "RunAndSignal");
  task.Run();
  event->Signal();
}
}  // namespace

void SingleThreadTaskRunner::PostBlockingTask(const base::Location& from_here,
                                              const base::Closure& task) {
  TRACE_EVENT0("task", "MessageLoop::PostBlockingTask");
  DCHECK(!BelongsToCurrentThread())
      << "PostBlockingTask can't be called from the MessageLoop's own thread. "
      << from_here.ToString();
  DCHECK(!task.is_null()) << from_here.ToString();

  base::WaitableEvent task_finished(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostTask(from_here,
           base::Bind(&RunAndSignal, task, base::Unretained(&task_finished)));

  // Wait for the task to complete before proceeding.
  task_finished.Wait();
}
#endif
}  // namespace base
