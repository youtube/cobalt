// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_THREAD_HELPERS_H_
#define STARBOARD_NPLB_THREAD_HELPERS_H_

#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

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

  SbMutex mutex;
  SbConditionVariable condition;
  SbConditionVariable return_condition;
  int unreturned_waiters;
};

// A semaphore that tests can use to trigger other threads
struct Semaphore {
  Semaphore();
  explicit Semaphore(int initial_value);
  ~Semaphore();

  void Put();
  void Take();

  SbMutex mutex;
  SbConditionVariable condition;
  int count;
};

// An aggregate type (which can be initialized with aggregate initialization) to
// be used with the TakeThenSignalEntryPoint.
struct TakeThenSignalContext {
  Semaphore do_signal;
  SbMutex mutex;
  SbConditionVariable condition;
  SbTime delay_after_signal;
};

// AbstractTestThread that is a bare bones class wrapper around Starboard
// thread. Subclasses must override Run().
class AbstractTestThread {
 public:
  AbstractTestThread() : thread_(kSbThreadInvalid) {}
  virtual ~AbstractTestThread() {}

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Calls SbThreadCreate() with default parameters.
  void Start() {
    SbThreadEntryPoint entry_point = ThreadEntryPoint;

    thread_ = SbThreadCreate(
        0,                     // default stack_size.
        kSbThreadNoPriority,   // default priority.
        kSbThreadNoAffinity,   // default affinity.
        true,                  // joinable.
        "AbstractTestThread",
        entry_point,
        this);

    if (kSbThreadInvalid == thread_) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Invalid thread.";
    }
    return;
  }

  void Join() {
    if (!SbThreadJoin(thread_, NULL)) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Could not join thread.";
    }
  }

 private:
  static void* ThreadEntryPoint(void* ptr) {
    AbstractTestThread* this_ptr = static_cast<AbstractTestThread*>(ptr);
    this_ptr->Run();
    return NULL;
  }

  SbThread thread_;

  SB_DISALLOW_COPY_AND_ASSIGN(AbstractTestThread);
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_THREAD_HELPERS_H_
