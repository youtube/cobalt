// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/scoped_set_task_priority_for_current_thread.h"

#include "base/compiler_specific.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#endif

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

ABSL_CONST_INIT SbOnceControl s_once_set_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_set_for_thread =
    kSbThreadLocalKeyInvalid;
void InitThreadLocalBoolKey() {
  s_thread_local_set_for_thread = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_set_for_thread));
}

void EnsureThreadLocalBoolKeyInited() {
  SbOnce(&s_once_set_flag, InitThreadLocalBoolKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_set_for_thread));
}

bool IsValueSetForThread() {
  void* set_for_thread = SbThreadGetLocalValue(s_thread_local_set_for_thread);
  return !!set_for_thread ? reinterpret_cast<intptr_t>(set_for_thread) != 0
                          : false;
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
  scoped_reset_value_ = reinterpret_cast<void*>(static_cast<intptr_t>(
                            static_cast<uint8_t>(GetTaskPriorityForCurrentThread())));

  SbThreadSetLocalValue(s_thread_local_key,
                        reinterpret_cast<void*>(static_cast<intptr_t>(
                            static_cast<uint8_t>(priority))));
  EnsureThreadLocalBoolKeyInited();
  SbThreadSetLocalValue(s_thread_local_set_for_thread,
                        reinterpret_cast<void*>(static_cast<intptr_t>(true)));
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
  SbThreadSetLocalValue(s_thread_local_key, scoped_reset_value_);
}
#else
ScopedSetTaskPriorityForCurrentThread::
    ~ScopedSetTaskPriorityForCurrentThread() = default;
#endif

TaskPriority GetTaskPriorityForCurrentThread() {
#if defined(STARBOARD)
  void* task_priority_for_current_thread =
      SbThreadGetLocalValue(s_thread_local_key);
  return IsValueSetForThread() ? static_cast<TaskPriority>(static_cast<uint8_t>(
                                     reinterpret_cast<intptr_t>(
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
