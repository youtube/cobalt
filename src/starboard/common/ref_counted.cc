// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/common/ref_counted.h"

#include "starboard/log.h"

namespace starboard {

namespace subtle {

RefCountedBase::RefCountedBase()
    : ref_count_(0)
#ifndef NDEBUG
      ,
      in_dtor_(false)
#endif
{
}

RefCountedBase::~RefCountedBase() {
#ifndef NDEBUG
  SB_DCHECK(in_dtor_) << "RefCounted object deleted without calling Release()";
#endif
}

void RefCountedBase::AddRef() const {
// TODO(maruel): Add back once it doesn't assert 500 times/sec.
// Current thread books the critical section "AddRelease" without release it.
// DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
#ifndef NDEBUG
  SB_DCHECK(!in_dtor_);
#endif
  ++ref_count_;
}

bool RefCountedBase::Release() const {
// TODO(maruel): Add back once it doesn't assert 500 times/sec.
// Current thread books the critical section "AddRelease" without release it.
// DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
#ifndef NDEBUG
  SB_DCHECK(!in_dtor_);
#endif
  if (--ref_count_ == 0) {
#ifndef NDEBUG
    in_dtor_ = true;
#endif
    return true;
  }
  return false;
}

bool RefCountedThreadSafeBase::HasOneRef() const {
  return (SbAtomicAcquire_Load(
              &const_cast<RefCountedThreadSafeBase*>(this)->ref_count_) == 1);
}

RefCountedThreadSafeBase::RefCountedThreadSafeBase() : ref_count_(0) {
#ifndef NDEBUG
  in_dtor_ = false;
#endif
}

RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
#ifndef NDEBUG
  SB_DCHECK(in_dtor_) << "RefCountedThreadSafe object deleted without "
                         "calling Release()";
#endif
}

void RefCountedThreadSafeBase::AddRef() const {
#ifndef NDEBUG
  SB_DCHECK(!in_dtor_);
#endif
  SbAtomicNoBarrier_Increment(&ref_count_, 1);
}

bool RefCountedThreadSafeBase::Release() const {
#ifndef NDEBUG
  SB_DCHECK(!in_dtor_);
  SB_DCHECK(!(SbAtomicAcquire_Load(&ref_count_) == 0));
#endif
  if (SbAtomicBarrier_Increment(&ref_count_, -1) == 0) {
#ifndef NDEBUG
    in_dtor_ = true;
#endif
    return true;
  }
  return false;
}

}  // namespace subtle

}  // namespace starboard
