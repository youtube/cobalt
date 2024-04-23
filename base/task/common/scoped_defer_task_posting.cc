// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/scoped_defer_task_posting.h"

#include "base/compiler_specific.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#endif

namespace base {

namespace {

#if defined(STARBOARD)
ABSL_CONST_INIT SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

ScopedDeferTaskPosting* GetScopedDeferTaskPosting() {
  return static_cast<ScopedDeferTaskPosting*>(
      SbThreadGetLocalValue(s_thread_local_key));
}
#else
// Holds a thread-local pointer to the current scope or null when no
// scope is active.
ABSL_CONST_INIT thread_local ScopedDeferTaskPosting* scoped_defer_task_posting =
    nullptr;
#endif

}  // namespace

// static
void ScopedDeferTaskPosting::PostOrDefer(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
  ScopedDeferTaskPosting* scope = Get();
  if (scope) {
    scope->DeferTaskPosting(std::move(task_runner), from_here, std::move(task),
                            delay);
    return;
  }

  task_runner->PostDelayedTask(from_here, std::move(task), delay);
}

// static
ScopedDeferTaskPosting* ScopedDeferTaskPosting::Get() {
#if defined(STARBOARD)
  return GetScopedDeferTaskPosting();
#else
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&scoped_defer_task_posting, sizeof(ScopedDeferTaskPosting*));

  return scoped_defer_task_posting;
#endif
}

// static
bool ScopedDeferTaskPosting::Set(ScopedDeferTaskPosting* scope) {
  // We can post a task from within a ScheduleWork in some tests, so we can
  // get nested scopes. In this case ignore all except the top one.
  if (Get() && scope)
    return false;
#if defined(STARBOARD)
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, scope);
#else
  scoped_defer_task_posting = scope;
#endif
  return true;
}

// static
bool ScopedDeferTaskPosting::IsPresent() {
  return !!Get();
}

ScopedDeferTaskPosting::ScopedDeferTaskPosting() {
  top_level_scope_ = Set(this);
}

ScopedDeferTaskPosting::~ScopedDeferTaskPosting() {
  if (!top_level_scope_) {
    DCHECK(deferred_tasks_.empty());
    return;
  }
  Set(nullptr);
  for (DeferredTask& deferred_task : deferred_tasks_) {
    deferred_task.task_runner->PostDelayedTask(deferred_task.from_here,
                                               std::move(deferred_task.task),
                                               deferred_task.delay);
  }
}

ScopedDeferTaskPosting::DeferredTask::DeferredTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    Location from_here,
    OnceClosure task,
    base::TimeDelta delay)
    : task_runner(std::move(task_runner)),
      from_here(from_here),
      task(std::move(task)),
      delay(delay) {}

ScopedDeferTaskPosting::DeferredTask::DeferredTask(DeferredTask&&) = default;

ScopedDeferTaskPosting::DeferredTask::~DeferredTask() = default;

void ScopedDeferTaskPosting::DeferTaskPosting(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
  deferred_tasks_.push_back(
      {std::move(task_runner), from_here, std::move(task), delay});
}

}  // namespace base
