// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#include <process.h>
#endif

#include "base/threading/simple_thread.h"
#include "base/threading/thread_local_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// Ignore warnings about ptr->int conversions that we use when
// storing ints into ThreadLocalStorage.
#pragma warning(disable : 4311 4312)
#endif

namespace base {

namespace {

const int kInitialTlsValue = 0x5555;
static ThreadLocalStorage::Slot tls_slot(LINKER_INITIALIZED);

class ThreadLocalStorageRunner : public DelegateSimpleThread::Delegate {
 public:
  explicit ThreadLocalStorageRunner(int* tls_value_ptr)
      : tls_value_ptr_(tls_value_ptr) {}

  virtual ~ThreadLocalStorageRunner() {}

  virtual void Run() {
    *tls_value_ptr_ = kInitialTlsValue;
    tls_slot.Set(tls_value_ptr_);

    int *ptr = static_cast<int*>(tls_slot.Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, kInitialTlsValue);
    *tls_value_ptr_ = 0;

    ptr = static_cast<int*>(tls_slot.Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, 0);
  }

 private:
  int* tls_value_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorageRunner);
};


void ThreadLocalStorageCleanup(void *value) {
  int *ptr = reinterpret_cast<int*>(value);
  if (ptr)
    *ptr = kInitialTlsValue;
}

}  // namespace

TEST(ThreadLocalStorageTest, Basics) {
  ThreadLocalStorage::Slot slot;
  slot.Set(reinterpret_cast<void*>(123));
  int value = reinterpret_cast<intptr_t>(slot.Get());
  EXPECT_EQ(value, 123);
}

TEST(ThreadLocalStorageTest, TLSDestructors) {
  // Create a TLS index with a destructor.  Create a set of
  // threads that set the TLS, while the destructor cleans it up.
  // After the threads finish, verify that the value is cleaned up.
  const int kNumThreads = 5;
  int values[kNumThreads];
  ThreadLocalStorageRunner* thread_delegates[kNumThreads];
  DelegateSimpleThread* threads[kNumThreads];

  tls_slot.Initialize(ThreadLocalStorageCleanup);

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; index++) {
    values[index] = kInitialTlsValue;
    thread_delegates[index] = new ThreadLocalStorageRunner(&values[index]);
    threads[index] = new DelegateSimpleThread(thread_delegates[index],
                                              "tls thread");
    threads[index]->Start();
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    threads[index]->Join();
    delete threads[index];
    delete thread_delegates[index];

    // Verify that the destructor was called and that we reset.
    EXPECT_EQ(values[index], kInitialTlsValue);
  }
}

}  // namespace base
