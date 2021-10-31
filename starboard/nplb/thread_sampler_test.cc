// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/atomic.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class CountingThread : public AbstractTestThread {
 public:
  ~CountingThread() { Stop(); }

  void Run() override {
    while (!stop_.load()) {
      counter_.increment();
      SbThreadSleep(kSbTimeMillisecond);
    }
  }

  void Stop() {
    stop_.store(true);
    Join();
  }

  int32_t GetCount() { return counter_.load(); }

  bool IsCounting(SbTime timeout) {
    int32_t end_count = GetCount() + 2;
    SbTime end_time = SbTimeGetNow() + timeout;
    while (SbTimeGetNow() < end_time) {
      if (GetCount() >= end_count) return true;
      SbThreadYield();
    }
    return false;
  }

 private:
  atomic_bool stop_;
  atomic_int32_t counter_;
};

TEST(ThreadSamplerTest, RainyDayCreateSamplerInvalidThread) {
  // Creating a sampler for an invalid thread should not succeed, and even
  // without without calling |SbThreadSamplerDelete| ASAN should not detect a
  // memory leak.
  SbThreadSampler sampler = SbThreadSamplerCreate(kSbThreadInvalid);
  EXPECT_FALSE(SbThreadSamplerIsValid(sampler));
}

TEST(ThreadSamplerTest, RainyDayDestroyInvalidSampler) {
  // Deleting an invalid sampler should be benign.
  SbThreadSamplerDestroy(kSbThreadSamplerInvalid);
}

TEST(ThreadSamplerTest, RainyDayFreezeInvalidSampler) {
  // Freezing an invalid sampler should not succeed.
  SbThreadContext context = SbThreadSamplerFreeze(kSbThreadSamplerInvalid);
  EXPECT_FALSE(SbThreadContextIsValid(context));
}

TEST(ThreadSamplerTest, RainyDayThawInvalidSampler) {
  // Thawing an invalid sampler should not succeed.
  EXPECT_FALSE(SbThreadSamplerThaw(kSbThreadSamplerInvalid));
}

TEST(ThreadSamplerTest, RainyDayInvalidContext) {
  // Getting properties from an invalid context should not succeed.
  void* ptr;
  // clang-format off
  EXPECT_FALSE(SbThreadContextGetPointer(
      kSbThreadContextInvalid, kSbThreadContextInstructionPointer, &ptr));
  EXPECT_FALSE(SbThreadContextGetPointer(
      kSbThreadContextInvalid, kSbThreadContextStackPointer, &ptr));
  EXPECT_FALSE(SbThreadContextGetPointer(
      kSbThreadContextInvalid, kSbThreadContextFramePointer, &ptr));
  // clang-format on
}

TEST(ThreadSamplerTest, CreateIfSupported) {
  CountingThread counting_thread;
  counting_thread.Start();

  SbThreadSampler sampler = SbThreadSamplerCreate(counting_thread.GetThread());

  EXPECT_EQ(SbThreadSamplerIsSupported(), SbThreadSamplerIsValid(sampler));

  SbThreadSamplerDestroy(sampler);
}

TEST(ThreadSamplerTest, SunnyDayThreadFreeze) {
  if (!SbThreadSamplerIsSupported()) {
    SB_LOG(WARNING) << "Skipping test, as there is no implementation.";
    return;
  }

  CountingThread counting_thread;
  counting_thread.Start();

  SbThreadSampler sampler = SbThreadSamplerCreate(counting_thread.GetThread());
  EXPECT_TRUE(SbThreadSamplerIsValid(sampler));

  // Check that the thread is counting, with a long timeout to avoid flakiness.
  EXPECT_TRUE(counting_thread.IsCounting(4 * kSbTimeSecond));

  // Check that the thread stops counting when frozen.
  EXPECT_NE(kSbThreadContextInvalid, SbThreadSamplerFreeze(sampler));
  EXPECT_FALSE(counting_thread.IsCounting(100 * kSbTimeMillisecond));

  // Check that the thread is counting again when thawed.
  EXPECT_TRUE(SbThreadSamplerThaw(sampler));
  EXPECT_TRUE(counting_thread.IsCounting(4 * kSbTimeSecond));

  SbThreadSamplerDestroy(sampler);
}

TEST(ThreadSamplerTest, SunnyDayThreadContextPointers) {
  if (!SbThreadSamplerIsSupported()) {
    SB_LOG(WARNING) << "Skipping test, as there is no implementation.";
    return;
  }

  CountingThread counting_thread;
  counting_thread.Start();

  SbThreadSampler sampler = SbThreadSamplerCreate(counting_thread.GetThread());
  EXPECT_TRUE(SbThreadSamplerIsValid(sampler));

  // Wait until the thread is counting before freezing it so that we know it's
  // in a valid stack frame.
  EXPECT_TRUE(counting_thread.IsCounting(4 * kSbTimeSecond));

  SbThreadContext ctx = SbThreadSamplerFreeze(sampler);
  EXPECT_TRUE(SbThreadContextIsValid(ctx));

  void* ip = nullptr;
  EXPECT_TRUE(
      SbThreadContextGetPointer(ctx, kSbThreadContextInstructionPointer, &ip));
  EXPECT_NE(nullptr, ip);

  void* sp = nullptr;
  EXPECT_TRUE(
      SbThreadContextGetPointer(ctx, kSbThreadContextStackPointer, &sp));
  EXPECT_NE(nullptr, sp);

  void* fp = nullptr;
  EXPECT_TRUE(
      SbThreadContextGetPointer(ctx, kSbThreadContextFramePointer, &fp));
  // X86-64 PSABI allows the frame pointer not to be stored in a register
  // because the compiled code can find the frame indexed on the stack, so we
  // may get a null FP if that's what happens to be in the register instead.
  // https://github.com/hjl-tools/x86-psABI/wiki/X86-psABI (see "stack frame")
  // EXPECT_NE(nullptr, fp);

  EXPECT_TRUE(SbThreadSamplerThaw(sampler));
  SbThreadSamplerDestroy(sampler);
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
