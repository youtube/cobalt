// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/task_runner_util.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "starboard/system.h"

namespace base {
namespace task_runner_util {

namespace {

#if defined(COBALT_BUILD_TYPE_DEBUG)
const int kTimeWaitInterval = 10000;
#else
const int kTimeWaitInterval = 2000;
#endif
}  // namespace

void WaitForFence(base::SequencedTaskRunner *task_runner,
                  const base::Location &from_here) {
  PostBlockingTask(task_runner, from_here, base::OnceClosure());
}

void PostBlockingTask(base::SequencedTaskRunner *task_runner,
                      const base::Location &from_here, base::OnceClosure task) {
  TRACE_EVENT1("base::task_runner_util", __func__, "from_here",
               from_here.ToString());
  DCHECK(!task_runner->RunsTasksInCurrentSequence())
      << __func__ << " can't be called from the current sequence, posted from "
      << from_here.ToString();

  base::WaitableEvent task_finished(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::OnceClosure closure = base::BindOnce(
      [](base::OnceClosure task, base::WaitableEvent *task_finished) -> void {
        if ((!task.is_null())) {
          std::move(task).Run();
        }
        task_finished->Signal();
      },
      std::move(task), &task_finished);
  bool task_may_run = task_runner->PostTask(FROM_HERE, std::move(closure));
  DCHECK(task_may_run) << "Task that will never run posted with " << __func__
                       << " from " << from_here.ToString();

  if (task_may_run) {
    // Wait for the task to complete before proceeding.
    do {
      if (task_finished.TimedWait(
              base::TimeDelta::FromMilliseconds(kTimeWaitInterval))) {
        break;
      }
#if !defined(COBALT_BUILD_TYPE_GOLD)
      if (!SbSystemIsDebuggerAttached()) {
        base::debug::StackTrace trace;
        std::string prefix(base::StringPrintf("[%s deadlock from %s]", __func__,
                                              from_here.ToString().c_str()));
        trace.PrintWithPrefix(prefix.c_str());
      }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
    } while (true);
  }
}

}  // namespace task_runner_util
}  // namespace base
