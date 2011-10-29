// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_impl.h"

#include <errno.h>

#include "base/logging.h"

namespace base {
namespace internal {

LockImpl::LockImpl() {
#ifdef LOCK_IMPL_CHECK_LIVENESS
  liveness_token_ = LT_ALIVE;
#endif

#ifndef NDEBUG
  // In debug, setup attributes for lock error checking.
  pthread_mutexattr_t mta;
  int rv = pthread_mutexattr_init(&mta);
  DCHECK_EQ(rv, 0);
  rv = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_ERRORCHECK);
  DCHECK_EQ(rv, 0);
  rv = pthread_mutex_init(&os_lock_, &mta);
  DCHECK_EQ(rv, 0);
  rv = pthread_mutexattr_destroy(&mta);
  DCHECK_EQ(rv, 0);
#else
  // In release, go with the default lock attributes.
  pthread_mutex_init(&os_lock_, NULL);
#endif
}

LockImpl::~LockImpl() {
#ifdef LOCK_IMPL_CHECK_LIVENESS
  CheckIsAlive();
  liveness_token_ = LT_DELETED;
#endif
  int rv = pthread_mutex_destroy(&os_lock_);
  DCHECK_EQ(rv, 0);
}

bool LockImpl::Try() {
#ifdef LOCK_IMPL_CHECK_LIVENESS
  CheckIsAlive();
#endif
  int rv = pthread_mutex_trylock(&os_lock_);
  DCHECK(rv == 0 || rv == EBUSY);
  return rv == 0;
}

void LockImpl::Lock() {
#ifdef LOCK_IMPL_CHECK_LIVENESS
  CheckIsAlive();
#endif
  int rv = pthread_mutex_lock(&os_lock_);
  DCHECK_EQ(rv, 0);
}

void LockImpl::Unlock() {
#ifdef LOCK_IMPL_CHECK_LIVENESS
  CheckIsAlive();
#endif
  int rv = pthread_mutex_unlock(&os_lock_);
  DCHECK_EQ(rv, 0);
}

#ifdef LOCK_IMPL_CHECK_LIVENESS
void LockImpl::CheckIsAlive() {
  CHECK_EQ(LT_ALIVE, liveness_token_)
      << "Lock was invalid. Please see: http://crbug.com/102161";
}
#endif

}  // namespace internal
}  // namespace base
