/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/concurrent_ptr.h"

#include "nb/simple_thread.h"
#include "starboard/atomic.h"
#include "starboard/common/semaphore.h"
#include "testing/gtest/include/gtest/gtest.h"

// Number of threads for the thread test.
#define NUM_THREADS 8

namespace nb {

using atomic_int32 = starboard::atomic_int32_t;
using atomic_int64 = starboard::atomic_int64_t;
using Semaphore = starboard::Semaphore;

struct CountLifetime {
  explicit CountLifetime(atomic_int32* shared_int) {
    shared_int_ = shared_int;
    shared_int_->increment();
  }

  ~CountLifetime() { shared_int_->decrement(); }

  atomic_int32* shared_int_;
};

TEST(ConcurrentPtr, nullptr) {
  ConcurrentPtr<int> concurrent_ptr(nullptr);
  ConcurrentPtr<int>::Access access_ptr =
      concurrent_ptr.access_ptr(SbThreadGetId());
  ASSERT_FALSE(access_ptr);
  ASSERT_FALSE(access_ptr.valid());
  ASSERT_EQ(access_ptr.get(), nullptr);
  access_ptr.release();
  concurrent_ptr.reset(nullptr);
}

TEST(ConcurrentPtr, UseSingleThreaded) {
  int* int_ptr = new int(1234);
  ConcurrentPtr<int> concurrent_ptr(int_ptr);
  ConcurrentPtr<int>::Access access_ptr =
      concurrent_ptr.access_ptr(SbThreadGetId());

  ASSERT_TRUE(access_ptr);
  ASSERT_TRUE(access_ptr.valid());
  ASSERT_EQ(int_ptr, access_ptr.get());
  ASSERT_EQ(int_ptr, &(*access_ptr));
  access_ptr.release();
  int* int_ptr2 = concurrent_ptr.release();
  ASSERT_EQ(int_ptr, int_ptr2);
  ASSERT_EQ(1234, *int_ptr);
  delete int_ptr;
}

TEST(ConcurrentPtr, ResetDeltesInternalObject) {
  atomic_int32 counter(0);

  CountLifetime* ptr = new CountLifetime(&counter);
  EXPECT_EQ(1, counter.load());

  ConcurrentPtr<CountLifetime> concurrent_ptr(ptr);
  concurrent_ptr.reset(nullptr);
  EXPECT_EQ(0, counter.load());
}

TEST(ConcurrentPtr, UsePointerOperators) {
  ConcurrentPtr<atomic_int32> concurrent_ptr(new atomic_int32());
  ConcurrentPtr<atomic_int32>::Access access_ptr =
      concurrent_ptr.access_ptr(SbThreadGetId());
  ASSERT_TRUE(access_ptr.valid());
  ASSERT_TRUE(access_ptr.get());

  {
    atomic_int32& int_ref = *access_ptr;
    ASSERT_EQ(0, int_ref.load());
  }

  ASSERT_EQ(0, access_ptr->load());
  access_ptr->store(2);
  ASSERT_EQ(2, access_ptr->load());
}

class ConcurrentPtrThreadWorker : public SimpleThread {
 public:
  ConcurrentPtrThreadWorker(Semaphore* thread_seal,
                            ConcurrentPtr<atomic_int32>* concurrent_ptr)
      : SimpleThread("ConcurrentPtrThreadWorker"),
        thread_seal_(thread_seal),
        concurrent_ptr_(concurrent_ptr),
        finished_(false) {
    Start();
  }

  ConcurrentPtrThreadWorker(const ConcurrentPtrThreadWorker&) = delete;

  ~ConcurrentPtrThreadWorker() {
    finished_ = true;
    Join();
  }

  virtual void Run() {
    thread_seal_->Take();  // Pause until the main thread unblocks us.
    int64_t yield_counter = 0;
    while (!finished_) {
      if (yield_counter++ % 512 == 0) {  // Be nice to other threads.
        SbThreadYield();
      }
      ConcurrentPtr<atomic_int32>::Access access_ptr =
          concurrent_ptr_->access_ptr(SbThreadGetId());
      if (access_ptr) {
        access_ptr->load();
        access_ptr->store(SbThreadGetId());
      }
    }
  }

  virtual void Cancel() { finished_ = true; }

 private:
  Semaphore* thread_seal_;
  ConcurrentPtr<atomic_int32>* concurrent_ptr_;
  bool finished_;
};

// Tests the expectation that the ConcurrentPtr can have it's underlying
// pointer repeatedly reset and that all the reading threads will be guaranteed
// to either be able to lock the pointer for it's use or be receive nullptr.
TEST(ConcurrentPtr, ThreadStressTest) {
  Semaphore thread_seal;
  ConcurrentPtr<atomic_int32> concurrent_ptr(new atomic_int32);

  std::vector<ConcurrentPtrThreadWorker*> threads;
  const int kThreads = NUM_THREADS;
  for (int i = 0; i < kThreads; ++i) {
    threads.push_back(
        new ConcurrentPtrThreadWorker(&thread_seal, &concurrent_ptr));
  }

  // Launch threads.
  for (int i = 0; i < kThreads; ++i) {
    thread_seal.Put();
  }

  // Repeatedly reset the underlying pointer. If there is a race
  // condition then the program will crash.
  const SbTime start_time = SbTimeGetNow();
  const SbTime end_time = start_time + (kSbTimeMillisecond * 100);
  while (SbTimeGetNow() < end_time) {
    concurrent_ptr.reset(nullptr);
    SbThreadYield();
    concurrent_ptr.reset(new atomic_int32);
    concurrent_ptr.reset(new atomic_int32);
    SbThreadYield();
  }

  for (auto it = threads.begin(); it != threads.end(); ++it) {
    delete *it;
  }
  threads.clear();
}

}  // namespace nb.
