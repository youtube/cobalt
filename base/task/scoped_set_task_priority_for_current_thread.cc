// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/scoped_set_task_priority_for_current_thread.h"

#include "base/compiler_specific.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace internal {

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
#else
ABSL_CONST_INIT thread_local TaskPriority task_priority_for_current_thread =
    TaskPriority::USER_BLOCKING;
#endif

}  // namespace

ScopedSetTaskPriorityForCurrentThread::ScopedSetTaskPriorityForCurrentThread(
    TaskPriority priority)
#if defined(STARBOARD)
{
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, reinterpret_cast<void*>(priority));
}
#else
    : resetter_(&task_priority_for_current_thread,
                priority,
                TaskPriority::USER_BLOCKING) {
}
#endif

#if defined(STARBOARD)
ScopedSetTaskPriorityForCurrentThread::
    ~ScopedSetTaskPriorityForCurrentThread() {
  SbThreadSetLocalValue(s_thread_local_key,
                        reinterpret_cast<void*>(TaskPriority::USER_BLOCKING));
}
#else
ScopedSetTaskPriorityForCurrentThread::
    ~ScopedSetTaskPriorityForCurrentThread() = default;
#endif

TaskPriority GetTaskPriorityForCurrentThread() {
#if defined(STARBOARD)
  void* task_priority_for_current_thread =
      SbThreadGetLocalValue(s_thread_local_key);
  return !!task_priority_for_current_thread
             ? static_cast<TaskPriority>(
                   static_cast<uint8_t>(reinterpret_cast<intptr_t>(
                       task_priority_for_current_thread)))
             : TaskPriority::USER_BLOCKING;
#else
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&task_priority_for_current_thread, sizeof(TaskPriority));

  return task_priority_for_current_thread;
#endif
}

}  // namespace internal
}  // namespace base
