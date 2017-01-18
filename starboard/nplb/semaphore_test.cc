// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/common/semaphore.h"
#include "starboard/nplb/thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

TEST(Semaphore, PutAndTake) {
  Semaphore semaphore;
  semaphore.Put();
  semaphore.Take();
  EXPECT_FALSE(semaphore.TakeTry());
}

TEST(Semaphore, PutAndTakeTry) {
  Semaphore semaphore;
  semaphore.Put();
  EXPECT_TRUE(semaphore.TakeTry());
}

TEST(Semaphore, InitialValue_One) {
  Semaphore semaphore(1);
  semaphore.Take();
  EXPECT_FALSE(semaphore.TakeTry());
}

class ThreadTakesSemaphore : public AbstractTestThread {
 public:
  explicit ThreadTakesSemaphore(Semaphore* s) : semaphore_(s) {}
  virtual void Run() SB_OVERRIDE { semaphore_->Take(); }

 private:
  Semaphore* semaphore_;
};
TEST(Semaphore, ThreadTakes) {
  Semaphore semaphore;
  ThreadTakesSemaphore thread(&semaphore);
  thread.Start();
  semaphore.Put();
  thread.Join();
}

class ThreadPutsSemaphore : public AbstractTestThread {
 public:
  explicit ThreadPutsSemaphore(Semaphore* s) : semaphore_(s) {}
  virtual void Run() SB_OVERRIDE { semaphore_->Put(); }

 private:
  Semaphore* semaphore_;
};
TEST(Semaphore, ThreadPuts) {
  Semaphore semaphore;
  ThreadPutsSemaphore thread(&semaphore);
  thread.Start();
  thread.Join();
  semaphore.Put();
}

}  // namespace nplb
}  // namespace starboard
