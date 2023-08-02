// Copyright 2023 The Cobalt Authors. All Rights Reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "glimp/thread_collision_warner.h"

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace nb {

void DCheckAsserter::warn() {
  SB_NOTREACHED() << "Thread Collision";
}

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

}  // namespace nb
