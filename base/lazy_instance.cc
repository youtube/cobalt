// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/threading/platform_thread.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"

namespace base {

bool LazyInstanceHelper::NeedsInstance() {
  // Try to create the instance, if we're the first, will go from EMPTY
  // to CREATING, otherwise we've already been beaten here.
  // The memory access has no memory ordering as STATE_EMPTY and STATE_CREATING
  // has no associated data (memory barriers are all about ordering
  // of memory accesses to *associated* data).
  if (base::subtle::NoBarrier_CompareAndSwap(
          &state_, STATE_EMPTY, STATE_CREATING) == STATE_EMPTY)
    // Caller must create instance
    return true;

  // It's either in the process of being created, or already created. Spin.
  // The load has acquire memory ordering as a thread which sees
  // state_ == STATE_CREATED needs to acquire visibility over
  // the associated data (buf_). Pairing Release_Store is in
  // CompleteInstance().
  while (base::subtle::Acquire_Load(&state_) != STATE_CREATED)
    PlatformThread::YieldCurrentThread();

  // Someone else created the instance.
  return false;
}

void LazyInstanceHelper::CompleteInstance(void* instance, void (*dtor)(void*)) {
  // See the comment to the corresponding HAPPENS_AFTER in Pointer().
  ANNOTATE_HAPPENS_BEFORE(&state_);

  // Instance is created, go from CREATING to CREATED.
  // Releases visibility over buf_ to readers. Pairing Acquire_Load's are in
  // NeedsInstance() and Pointer().
  base::subtle::Release_Store(&state_, STATE_CREATED);

  // Make sure that the lazily instantiated object will get destroyed at exit.
  if (dtor)
    base::AtExitManager::RegisterCallback(dtor, instance);
}

}  // namespace base



