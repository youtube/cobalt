// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_
#define BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_

#include "base/base_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/threading/thread_checker.h"

namespace base::mac {

// ScopedNSAutoreleasePool creates an autorelease pool when instantiated and
// pops it when destroyed.  This allows an autorelease pool to be maintained in
// ordinary C++ code without bringing in any direct Objective-C dependency.
//
// Before using, please be aware that the semantics of autorelease pools do not
// match the semantics of a C++ class. In particular, recycling or destructing a
// pool lower on the stack destroys all pools higher on the stack, which does
// not mesh well with the existence of C++ objects for each pool.
//
// TODO(https://crbug.com/1424190): Enforce stack-only use via the
// STACK_ALLOCATED annotation.
//
// Use this class only in C++ code; use @autoreleasepool in Obj-C(++) code.

class BASE_EXPORT ScopedNSAutoreleasePool {
 public:
  ScopedNSAutoreleasePool();

  ScopedNSAutoreleasePool(const ScopedNSAutoreleasePool&) = delete;
  ScopedNSAutoreleasePool& operator=(const ScopedNSAutoreleasePool&) = delete;
  ScopedNSAutoreleasePool(ScopedNSAutoreleasePool&&) = delete;
  ScopedNSAutoreleasePool& operator=(ScopedNSAutoreleasePool&&) = delete;

  ~ScopedNSAutoreleasePool();

  // Clear out the pool in case its position on the stack causes it to be alive
  // for long periods of time (such as the entire length of the app). Only use
  // then when you're certain the items currently in the pool are no longer
  // needed.
  void Recycle();

 private:
  // This field is not a raw_ptr<> because it is a pointer to an Objective-C
  // object.
  RAW_PTR_EXCLUSION void* autorelease_pool_ GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

}  // namespace base::mac

#endif  // BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_
