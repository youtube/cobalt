// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/common/thread_collision_warner.h"

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace starboard {

AsserterBase::~AsserterBase() {}

DCheckAsserter::~DCheckAsserter() {}

void DCheckAsserter::warn() {
  SB_NOTREACHED() << "Thread Collision";
}

ThreadCollisionWarner::ThreadCollisionWarner(AsserterBase* asserter)
    : valid_thread_id_(0), counter_(0), asserter_(asserter) {}

ThreadCollisionWarner::~ThreadCollisionWarner() {
  delete asserter_;
}

ThreadCollisionWarner::Check::Check(ThreadCollisionWarner* warner)
    : warner_(warner) {
  warner_->EnterSelf();
}

ThreadCollisionWarner::Check::~Check() {}

static SbAtomic32 CurrentThread() {
  const SbThreadId current_thread_id = SbThreadGetId();
  // We need to get the thread id into an atomic data type. This might be a
  // truncating conversion, but any loss-of-information just increases the
  // chance of a false negative, not a false positive.
  const SbAtomic32 atomic_thread_id =
      static_cast<SbAtomic32>(current_thread_id);
  return atomic_thread_id;
}

void ThreadCollisionWarner::EnterSelf() {
  // If the active thread is 0 then I'll write the current thread ID
  // if two or more threads arrive here only one will succeed to
  // write on valid_thread_id_ the current thread ID.
  SbAtomic32 current_thread_id = CurrentThread();

  int previous_value =
      SbAtomicNoBarrier_CompareAndSwap(&valid_thread_id_, 0, current_thread_id);
  if (previous_value != 0 && previous_value != current_thread_id) {
    // gotcha! a thread is trying to use the same class and that is
    // not current thread.
    asserter_->warn();
  }

  SbAtomicNoBarrier_Increment(&counter_, 1);
}

void ThreadCollisionWarner::Enter() {
  SbAtomic32 current_thread_id = CurrentThread();

  if (SbAtomicNoBarrier_CompareAndSwap(&valid_thread_id_, 0,
                                       current_thread_id) != 0) {
    // gotcha! another thread is trying to use the same class.
    asserter_->warn();
  }

  SbAtomicNoBarrier_Increment(&counter_, 1);
}

void ThreadCollisionWarner::Leave() {
  if (SbAtomicBarrier_Increment(&counter_, -1) == 0) {
    SbAtomicNoBarrier_Store(&valid_thread_id_, 0);
  }
}

}  // namespace starboard
