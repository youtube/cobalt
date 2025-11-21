// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_THREAD_HELPERS_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_THREAD_HELPERS_H_

#include <pthread.h>

#include <cstddef>
#include <cstdint>

#include "starboard/configuration.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {

class TestSemaphore {
 public:
  TestSemaphore();  // initial_thread_permits = 0;
  explicit TestSemaphore(int initial_thread_permits);

  ~TestSemaphore();

  // Increases the permits. One thread will be woken up if it is blocked in
  // Take().
  void Put();

  // The caller is blocked if the counter is negative, and will stay blocked
  // until Put() is invoked by another thread. The permits is then
  // decremented by one.

  void Take();

  // A non-blocking version of Take(). If the counter is negative then this
  // function returns immediately and the semaphore is not modified. If true
  // is returned then the effects are the same as Take().
  bool TakeTry();

 private:
  pthread_mutex_t mutex_;
  pthread_cond_t condition_;
  int permits_;
};

inline TestSemaphore::TestSemaphore() : permits_(0) {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&condition_, NULL);
}

inline TestSemaphore::TestSemaphore(int initial_thread_permits)
    : permits_(initial_thread_permits) {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&condition_, NULL);
}

inline TestSemaphore::~TestSemaphore() {
  pthread_mutex_destroy(&mutex_);
  pthread_cond_destroy(&condition_);
}

inline void TestSemaphore::Put() {
  pthread_mutex_lock(&mutex_);
  ++permits_;
  pthread_cond_signal(&condition_);
  pthread_mutex_unlock(&mutex_);
}

inline void TestSemaphore::Take() {
  pthread_mutex_lock(&mutex_);
  while (permits_ <= 0) {
    pthread_cond_wait(&condition_, &mutex_);
  }
  --permits_;
  pthread_mutex_unlock(&mutex_);
}

inline bool TestSemaphore::TakeTry() {
  pthread_mutex_lock(&mutex_);
  if (permits_ <= 0) {
    pthread_mutex_unlock(&mutex_);
    return false;
  }
  --permits_;
  pthread_mutex_unlock(&mutex_);
  return true;
}

inline void* ToVoid(intptr_t value) {
  return reinterpret_cast<void*>(value);
}

inline intptr_t FromVoid(void* value) {
  return reinterpret_cast<intptr_t>(value);
}

void* const kSomeContext = ToVoid(1234);
void* const kSomeContextPlusOne = ToVoid(1235);

const char* const kThreadName = "ThreadName";
const char* const kAltThreadName = "AltThreadName";

// Does not yield, can't you read?
void DoNotYield();

// Adds 1 to the input.
void* AddOneEntryPoint(void* context);

// Acquires the mutex from the WaiterContext, signals the return_condition, then
// waits on the condition.
void* WaiterEntryPoint(void* context);

// Takes a token from the TakeThenSignalContext semaphore, and then signals the
// condition variable.
void* TakeThenSignalEntryPoint(void* context);

// A simple struct that will initialize its members on construction and destroy
// them on destruction.
struct WaiterContext {
  WaiterContext();
  ~WaiterContext();

  // Waits on |condition|, incrementing |unreturned_waiters| and signalling
  // |return_condition|.
  void Wait();

  // Waits until unreturned_waiters > 0, and then takes one.
  void WaitForReturnSignal();

  pthread_mutex_t mutex;
  pthread_cond_t condition;
  pthread_cond_t return_condition;
  int unreturned_waiters;
};

// An aggregate type (which can be initialized with aggregate initialization) to
// be used with the TakeThenSignalEntryPoint.
struct TakeThenSignalContext {
  TestSemaphore do_signal;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  int64_t delay_after_signal;
};

// AbstractTestThread that is a bare bones class wrapper around Starboard
// thread. Subclasses must override Run().
class AbstractTestThread {
 public:
  AbstractTestThread() : thread_(0) {}
  virtual ~AbstractTestThread() {}

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Calls pthread_create() with default parameters.
  void Start() {
    pthread_create(&thread_, NULL, ThreadEntryPoint, this);
    if (0 == thread_) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Invalid thread.";
    }
    return;
  }

  void Join() {
    if (pthread_join(thread_, NULL) != 0) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Could not join thread.";
    }
  }

  pthread_t GetThread() { return thread_; }

 private:
  static void* ThreadEntryPoint(void* ptr) {
#if defined(__APPLE__)
    pthread_setname_np("AbstractTestThread");
#else
    pthread_setname_np(pthread_self(), "AbstractTestThread");
#endif
    AbstractTestThread* this_ptr = static_cast<AbstractTestThread*>(ptr);
    this_ptr->Run();
    return NULL;
  }

  pthread_t thread_;

  AbstractTestThread(const AbstractTestThread&) = delete;
  void operator=(const AbstractTestThread&) = delete;
};

}  // namespace nplb

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_THREAD_HELPERS_H_
