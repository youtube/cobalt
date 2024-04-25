// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/default_delayed_task_handle_delegate.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include <pthread.h>

#include "base/check_op.h"
#include "starboard/thread.h"
#endif

namespace base {

namespace {

#if defined(STARBOARD)
ABSL_CONST_INIT pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

SequencedTaskRunner::CurrentDefaultHandle* GetCurrentDefaultHandle() {
  return static_cast<SequencedTaskRunner::CurrentDefaultHandle*>(
      SbThreadGetLocalValue(s_thread_local_key));
}
#else
ABSL_CONST_INIT thread_local SequencedTaskRunner::CurrentDefaultHandle*
    current_default_handle = nullptr;
#endif

}  // namespace

bool SequencedTaskRunner::PostNonNestableTask(const Location& from_here,
                                              OnceClosure task) {
  return PostNonNestableDelayedTask(from_here, std::move(task),
                                    base::TimeDelta());
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTask(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  auto delayed_task_handle_delegate =
      std::make_unique<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  PostDelayedTask(from_here, std::move(task), delay);

  return delayed_task_handle;
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTaskAt(
    subtle::PostDelayedTaskPassKey pass_key,
    const Location& from_here,
    OnceClosure task,
    TimeTicks delayed_run_time,
    subtle::DelayPolicy deadline_policy) {
  auto delayed_task_handle_delegate =
      std::make_unique<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  if (!PostDelayedTaskAt(pass_key, from_here, std::move(task), delayed_run_time,
                         deadline_policy)) {
    DCHECK(!delayed_task_handle.IsValid());
  }
  return delayed_task_handle;
}

bool SequencedTaskRunner::PostDelayedTaskAt(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure task,
    TimeTicks delayed_run_time,
    subtle::DelayPolicy deadline_policy) {
  return PostDelayedTask(from_here, std::move(task),
                         delayed_run_time.is_null()
                             ? base::TimeDelta()
                             : delayed_run_time - TimeTicks::Now());
}

// static
const scoped_refptr<SequencedTaskRunner>&
SequencedTaskRunner::GetCurrentDefault() {
#if defined(STARBOARD)
  auto current_default_handle = GetCurrentDefaultHandle();
#endif
  CHECK(current_default_handle)
      << "Error: This caller requires a sequenced context (i.e. the current "
         "task needs to run from a SequencedTaskRunner). If you're in a test "
         "refer to //docs/threading_and_tasks_testing.md.";
  return current_default_handle->task_runner_;
}

// static
bool SequencedTaskRunner::HasCurrentDefault() {
#if defined(STARBOARD)
  auto current_default_handle = GetCurrentDefaultHandle();
#endif
  return !!current_default_handle;
}

SequencedTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SequencedTaskRunner> task_runner)
#if defined(STARBOARD)
    :
#else
    : resetter_(&current_default_handle, this, nullptr),
#endif
      task_runner_(std::move(task_runner)) {
#if defined(STARBOARD)
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, this);
#endif
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

SequencedTaskRunner::CurrentDefaultHandle::~CurrentDefaultHandle() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
#if defined(STARBOARD)
  auto current_default_handle = GetCurrentDefaultHandle();
  SbThreadSetLocalValue(s_thread_local_key, nullptr);
#endif
  DCHECK_EQ(current_default_handle, this);
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
