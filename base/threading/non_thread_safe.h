// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_NON_THREAD_SAFE_H_
#define BASE_THREADING_NON_THREAD_SAFE_H_
#pragma once

#include "base/threading/thread_checker.h"

namespace base {

// A helper class used to help verify that methods of a class are
// called from the same thread.  One can inherit from this class and use
// CalledOnValidThread() to verify.
//
// This is intended to be used with classes that appear to be thread safe, but
// aren't.  For example, a service or a singleton like the preferences system.
//
// Example:
// class MyClass : public base::NonThreadSafe {
//  public:
//   void Foo() {
//     DCHECK(CalledOnValidThread());
//     ... (do stuff) ...
//   }
// }
//
// In Release mode, CalledOnValidThread will always return true.
//
#ifndef NDEBUG
class NonThreadSafe {
 public:
  ~NonThreadSafe();

  bool CalledOnValidThread() const;

 protected:
  // Changes the thread that is checked for in CalledOnValidThread. The next
  // call to CalledOnValidThread will attach this class to a new thread. It is
  // up to the NonThreadSafe derived class to decide to expose this or not.
  // This may be useful when an object may be created on one thread and then
  // used exclusively on another thread.
  void DetachFromThread();

 private:
  ThreadChecker thread_checker_;
};
#else
// Do nothing in release mode.
class NonThreadSafe {
 public:
  bool CalledOnValidThread() const {
    return true;
  }

 protected:
  void DetachFromThread() {}
};
#endif  // NDEBUG

}  // namespace base

#endif  // BASE_NON_THREAD_SAFE_H_
