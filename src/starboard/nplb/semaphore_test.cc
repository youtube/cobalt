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

// Tests the expectation that waiting no time will succeed
// If the semaphore is uncontended.
TEST(Semaphore, TakeWait_Zero) {
  Semaphore semaphore;
  semaphore.Put();
  EXPECT_TRUE(semaphore.TakeWait(0));
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

class ThreadTakesWaitSemaphore : public AbstractTestThread {
 public:
  explicit ThreadTakesWaitSemaphore(SbTime wait_us)
      : wait_us_(wait_us), result_signaled_(false),
        result_wait_time_(0) {}
  virtual void Run() SB_OVERRIDE {
    SbTime start_time = SbTimeGetMonotonicNow();
    result_signaled_ = semaphore_.TakeWait(wait_us_);
    result_wait_time_ = SbTimeGetMonotonicNow() - start_time;
  }

  SbTime wait_us_;
  Semaphore semaphore_;
  bool result_signaled_;
  SbTime result_wait_time_;
};

TEST(Semaphore, ThreadTakesWait_PutBeforeTimeExpires) {
  SbTime wait_time = kSbTimeMillisecond * 80;
  ThreadTakesWaitSemaphore thread(wait_time);
  thread.Start();

  SbThreadSleep(wait_time / 2);
  thread.semaphore_.Put();

  thread.Join();

  EXPECT_TRUE(thread.result_signaled_);
  EXPECT_NEAR(thread.result_wait_time_,
              wait_time / 2,
              kSbTimeMillisecond * 10);  // Error threshold
}

TEST(Semaphore, ThreadTakesWait_TimeExpires) {
  SbTime wait_time = kSbTimeMillisecond * 20;
  ThreadTakesWaitSemaphore thread(wait_time);
  thread.Start();

  SbThreadSleep(wait_time * 2);
  thread.semaphore_.Put();

  thread.Join();

  EXPECT_FALSE(thread.result_signaled_);
  EXPECT_NEAR(thread.result_wait_time_,
              wait_time,
              kSbTimeMillisecond * 5);  // Error threshold
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
