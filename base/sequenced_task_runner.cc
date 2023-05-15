// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequenced_task_runner.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"

#include <utility>

#include "base/bind.h"

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

void SequencedTaskRunner::PostBlockingTask(const base::Location& from_here,
                                              const base::Closure& task) {
  TRACE_EVENT0("task", "MessageLoop::PostBlockingTask");
  DCHECK(!RunsTasksInCurrentSequence())
      << "PostBlockingTask can't be called from the MessageLoop's own thread. "
      << from_here.ToString();
  DCHECK(!task.is_null()) << from_here.ToString();

  base::WaitableEvent task_finished(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool task_may_run = PostTask(from_here,
           base::Bind(&RunAndSignal, task, base::Unretained(&task_finished)));
  DCHECK(task_may_run)
      << "Task that will never run posted with PostBlockingTask.";

  if (task_may_run) {
    // Wait for the task to complete before proceeding.
    task_finished.Wait();
  }
}
#endif

bool SequencedTaskRunner::PostNonNestableTask(const Location& from_here,
                                              OnceClosure task) {
  return PostNonNestableDelayedTask(from_here, std::move(task),
                                    base::TimeDelta());
}

bool SequencedTaskRunner::DeleteOrReleaseSoonInternal(
    const Location& from_here,
    void (*deleter)(const void*),
    const void* object) {
  return PostNonNestableTask(from_here, BindOnce(deleter, object));
}

OnTaskRunnerDeleter::OnTaskRunnerDeleter(
    scoped_refptr<SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
}

OnTaskRunnerDeleter::~OnTaskRunnerDeleter() = default;

OnTaskRunnerDeleter::OnTaskRunnerDeleter(OnTaskRunnerDeleter&&) = default;

OnTaskRunnerDeleter& OnTaskRunnerDeleter::operator=(
    OnTaskRunnerDeleter&&) = default;

}  // namespace base
