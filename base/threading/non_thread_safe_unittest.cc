// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Simple class to exersice the basics of NonThreadSafe.
// Both the destructor and DoStuff should verify that they were
// called on the same thread as the constructor.
class NonThreadSafeClass : public NonThreadSafe {
 public:
  NonThreadSafeClass() {}

  // Verifies that it was called on the same thread as the constructor.
  void DoStuff() {
    CHECK(CalledOnValidThread());
  }

  void DetachFromThread() {
    NonThreadSafe::DetachFromThread();
  }

  static void MethodOnDifferentThreadImpl();
  static void DestructorOnDifferentThreadImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(NonThreadSafeClass);
};

// Calls NonThreadSafeClass::DoStuff on another thread.
class CallDoStuffOnThread : public SimpleThread {
 public:
  CallDoStuffOnThread(NonThreadSafeClass* non_thread_safe_class)
      : SimpleThread("call_do_stuff_on_thread"),
        non_thread_safe_class_(non_thread_safe_class) {
  }

  virtual void Run() {
    non_thread_safe_class_->DoStuff();
  }

 private:
  NonThreadSafeClass* non_thread_safe_class_;

  DISALLOW_COPY_AND_ASSIGN(CallDoStuffOnThread);
};

// Deletes NonThreadSafeClass on a different thread.
class DeleteNonThreadSafeClassOnThread : public SimpleThread {
 public:
  DeleteNonThreadSafeClassOnThread(NonThreadSafeClass* non_thread_safe_class)
      : SimpleThread("delete_non_thread_safe_class_on_thread"),
        non_thread_safe_class_(non_thread_safe_class) {
  }

  virtual void Run() {
    non_thread_safe_class_.reset();
  }

 private:
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class_;

  DISALLOW_COPY_AND_ASSIGN(DeleteNonThreadSafeClassOnThread);
};

TEST(NonThreadSafeTest, CallsAllowedOnSameThread) {
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
      new NonThreadSafeClass);

  // Verify that DoStuff doesn't assert.
  non_thread_safe_class->DoStuff();

  // Verify that the destructor doesn't assert.
  non_thread_safe_class.reset();
}

TEST(NonThreadSafeTest, DetachThenDestructOnDifferentThread) {
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
      new NonThreadSafeClass);

  // Verify that the destructor doesn't assert when called on a different thread
  // after a detach.
  non_thread_safe_class->DetachFromThread();
  DeleteNonThreadSafeClassOnThread delete_on_thread(
      non_thread_safe_class.release());

  delete_on_thread.Start();
  delete_on_thread.Join();
}

#if GTEST_HAS_DEATH_TEST || NDEBUG

void NonThreadSafeClass::MethodOnDifferentThreadImpl() {
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
      new NonThreadSafeClass);

  // Verify that DoStuff asserts in debug builds only when called
  // on a different thread.
  CallDoStuffOnThread call_on_thread(non_thread_safe_class.get());

  call_on_thread.Start();
  call_on_thread.Join();
}

#ifndef NDEBUG
TEST(NonThreadSafeDeathTest, MethodNotAllowedOnDifferentThreadInDebug) {
  ASSERT_DEBUG_DEATH({
      NonThreadSafeClass::MethodOnDifferentThreadImpl();
    }, "");
}
#else
TEST(NonThreadSafeTest, MethodAllowedOnDifferentThreadInRelease) {
  NonThreadSafeClass::MethodOnDifferentThreadImpl();
}
#endif  // NDEBUG

void NonThreadSafeClass::DestructorOnDifferentThreadImpl() {
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
      new NonThreadSafeClass);

  // Verify that the destructor asserts in debug builds only
  // when called on a different thread.
  DeleteNonThreadSafeClassOnThread delete_on_thread(
      non_thread_safe_class.release());

  delete_on_thread.Start();
  delete_on_thread.Join();
}

#ifndef NDEBUG
TEST(NonThreadSafeDeathTest, DestructorNotAllowedOnDifferentThreadInDebug) {
  ASSERT_DEBUG_DEATH({
      NonThreadSafeClass::DestructorOnDifferentThreadImpl();
    }, "");
}
#else
TEST(NonThreadSafeTest, DestructorAllowedOnDifferentThreadInRelease) {
  NonThreadSafeClass::DestructorOnDifferentThreadImpl();
}
#endif  // NDEBUG

#endif  // GTEST_HAS_DEATH_TEST || NDEBUG

}  // namespace base
