// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_threadsafe.h"
#include "base/compiler_specific.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include "starboard/once.h"
#endif

namespace base {
namespace internal {

#if defined(STARBOARD)

namespace {

ABSL_CONST_INIT SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

}

void ObserverListThreadSafeBase::EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

const SbThreadLocalKey ObserverListThreadSafeBase::GetThreadLocalKey() {
  return s_thread_local_key;
}

#else
ABSL_CONST_INIT thread_local const ObserverListThreadSafeBase::
    NotificationDataBase* current_notification = nullptr;
#endif

// static
const ObserverListThreadSafeBase::NotificationDataBase*&
ObserverListThreadSafeBase::GetCurrentNotification() {
#if defined(STARBOARD)
  auto& ptr = *static_cast<const ObserverListThreadSafeBase::NotificationDataBase**>(
      SbThreadGetLocalValue(s_thread_local_key));
  return ptr;
#else
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(
      &current_notification,
      sizeof(const ObserverListThreadSafeBase::NotificationDataBase*));

  return current_notification;
#endif
}

}  // namespace internal
}  // namespace base
