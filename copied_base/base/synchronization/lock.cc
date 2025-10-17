// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/synchronization/lock.h"

#if DCHECK_IS_ON()
#include "base/threading/platform_thread.h"
#endif

#if DCHECK_IS_ON()

namespace base {

Lock::Lock() : lock_() {
}

Lock::~Lock() {
  DCHECK(owning_thread_ref_.is_null());
}

void Lock::AssertAcquired() const {
  DCHECK_EQ(owning_thread_ref_, PlatformThread::CurrentRef());
}

void Lock::AssertNotHeld() const {
  DCHECK(owning_thread_ref_.is_null());
}

void Lock::CheckHeldAndUnmark() {
  DCHECK_EQ(owning_thread_ref_, PlatformThread::CurrentRef());
  owning_thread_ref_ = PlatformThreadRef();
}

void Lock::CheckUnheldAndMark() {
  DCHECK(owning_thread_ref_.is_null());
  owning_thread_ref_ = PlatformThread::CurrentRef();
}

}  // namespace base

#endif  // DCHECK_IS_ON()
