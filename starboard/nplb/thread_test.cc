// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <functional>

#include "starboard/atomic.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class TestRunThread : public Thread {
 public:
  TestRunThread() : Thread("TestThread"), finished_(false) {}

  void Run() override {
    while (!WaitForJoin(kSbTimeMillisecond)) {
    }
    finished_.store(true);
  }
  atomic_bool finished_;
};

// Tests the expectation that a thread subclass will have the expected
// behavior of running the Run() function will execute only after
// Start(), and will exit on Join().
TEST(Thread, TestRunThread) {
  TestRunThread test_thread;
  SbThreadSleep(100);
  // Expect that test thread not in a run state when initialized.
  EXPECT_FALSE(test_thread.finished_.load());
  EXPECT_FALSE(test_thread.join_called());

  test_thread.Start();
  EXPECT_FALSE(test_thread.finished_.load());
  EXPECT_FALSE(test_thread.join_called());

  test_thread.Join();
  EXPECT_TRUE(test_thread.finished_.load());
  EXPECT_TRUE(test_thread.join_called());
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
