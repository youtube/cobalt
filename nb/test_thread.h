// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef NB_TEST_THREAD_H_
#define NB_TEST_THREAD_H_

#include "starboard/configuration.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {

// TestThread that is a bare bones class wrapper around Starboard
// thread. Subclasses must override Run().
class TestThread {
 public:
  TestThread() : thread_(kSbThreadInvalid) {}
  virtual ~TestThread() {}

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Calls SbThreadCreate() with default parameters.
  void Start() {
    SbThreadEntryPoint entry_point = ThreadEntryPoint;

    thread_ = SbThreadCreate(0,                    // default stack_size.
                             kSbThreadNoPriority,  // default priority.
                             kSbThreadNoAffinity,  // default affinity.
                             true,                 // joinable.
                             "TestThread", entry_point, this);

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
    TestThread* this_ptr = static_cast<TestThread*>(ptr);
    this_ptr->Run();
    return NULL;
  }

  SbThread thread_;

  TestThread(const TestThread&) = delete;
  void operator=(const TestThread&) = delete;
};

}  // namespace nb.

#endif  // NB_TEST_THREAD_H_
