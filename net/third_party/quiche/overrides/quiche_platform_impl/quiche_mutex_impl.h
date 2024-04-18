// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MUTEX_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MUTEX_IMPL_H_

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_export.h"

#define QUICHE_EXCLUSIVE_LOCKS_REQUIRED_IMPL EXCLUSIVE_LOCKS_REQUIRED
#define QUICHE_GUARDED_BY_IMPL GUARDED_BY
#define QUICHE_LOCKABLE_IMPL LOCKABLE
#define QUICHE_LOCKS_EXCLUDED_IMPL LOCKS_EXCLUDED
#define QUICHE_SHARED_LOCKS_REQUIRED_IMPL SHARED_LOCKS_REQUIRED
#define QUICHE_EXCLUSIVE_LOCK_FUNCTION_IMPL EXCLUSIVE_LOCK_FUNCTION
#define QUICHE_UNLOCK_FUNCTION_IMPL UNLOCK_FUNCTION
#define QUICHE_SHARED_LOCK_FUNCTION_IMPL SHARED_LOCK_FUNCTION
#define QUICHE_SCOPED_LOCKABLE_IMPL SCOPED_LOCKABLE
#define QUICHE_ASSERT_SHARED_LOCK_IMPL ASSERT_SHARED_LOCK

#ifndef EXCLUSIVE_LOCK_FUNCTION
#define EXCLUSIVE_LOCK_FUNCTION(...)
#endif

#ifndef UNLOCK_FUNCTION
#define UNLOCK_FUNCTION(...)
#endif

#ifndef SHARED_LOCK_FUNCTION
#define SHARED_LOCK_FUNCTION(...)
#endif

#ifndef ASSERT_SHARED_LOCK
#define ASSERT_SHARED_LOCK(...)
#endif

#ifndef LOCKABLE
#define LOCKABLE
#endif

#ifndef SCOPED_LOCKABLE
#define SCOPED_LOCKABLE
#endif

#ifndef GUARDED_BY
#define GUARDED_BY(x)
#endif

#ifndef SHARED_LOCKS_REQUIRED
#define SHARED_LOCKS_REQUIRED(...)
#endif

#ifndef EXCLUSIVE_LOCKS_REQUIRED
#define EXCLUSIVE_LOCKS_REQUIRED(...)
#endif

namespace quiche {

// A class wrapping a non-reentrant mutex.
class QUICHE_LOCKABLE_IMPL QUICHE_EXPORT QuicheLockImpl {
 public:
  QuicheLockImpl() = default;

  QuicheLockImpl(const QuicheLockImpl&) = delete;
  QuicheLockImpl& operator=(const QuicheLockImpl&) = delete;

  // Block until lock_ is free, then acquire it exclusively.
  void WriterLock() EXCLUSIVE_LOCK_FUNCTION();

  // Release lock_. Caller must hold it exclusively.
  void WriterUnlock() UNLOCK_FUNCTION();

  // Block until lock_ is free or shared, then acquire a share of it.
  void ReaderLock() SHARED_LOCK_FUNCTION();

  // Release lock_. Caller could hold it in shared mode.
  void ReaderUnlock() UNLOCK_FUNCTION();

  // Not implemented.
  void AssertReaderHeld() const ASSERT_SHARED_LOCK() {}

 private:
  base::Lock lock_;
};

// A Notification allows threads to receive notification of a single occurrence
// of a single event.
class QUICHE_EXPORT QuicheNotificationImpl {
 public:
  QuicheNotificationImpl()
      : event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  QuicheNotificationImpl(const QuicheNotificationImpl&) = delete;
  QuicheNotificationImpl& operator=(const QuicheNotificationImpl&) = delete;

  bool HasBeenNotified() { return event_.IsSignaled(); }

  void Notify() { event_.Signal(); }

  void WaitForNotification() { event_.Wait(); }

 private:
  base::WaitableEvent event_;
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MUTEX_IMPL_H_
