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

#include <pthread.h>
#include <sys/time.h>

#include "starboard/common/time.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void InitCondition(pthread_cond_t* condition, bool use_monotonic) {
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  pthread_condattr_t attribute;
  EXPECT_EQ(pthread_condattr_init(&attribute), 0);
  if (use_monotonic) {
    EXPECT_EQ(pthread_condattr_setclock(&attribute, CLOCK_MONOTONIC), 0);
  }
  EXPECT_EQ(pthread_cond_init(condition, &attribute), 0);
  EXPECT_EQ(pthread_condattr_destroy(&attribute), 0);
#else
  EXPECT_EQ(pthread_cond_init(condition, NULL), 0);
#endif
}

int64_t CurrentTime(bool use_monotonic) {
  if (use_monotonic) {
    return starboard::CurrentMonotonicTime();
  } else {
    return starboard::CurrentPosixTime();
  }
}

struct timespec CalculateDelayTimestamp(int64_t delay_us, bool use_monotonic) {
  int64_t timeout_time_usec = 0;
  if (use_monotonic) {
    timeout_time_usec = starboard::CurrentMonotonicTime();
  } else {
    timeout_time_usec = starboard::CurrentPosixTime();
  }
  timeout_time_usec += delay_us;
  EXPECT_GT(timeout_time_usec, 0);

  struct timespec delay_timestamp;
  delay_timestamp.tv_sec = timeout_time_usec / 1'000'000;
  delay_timestamp.tv_nsec = (timeout_time_usec % 1'000'000) * 1000;

  return delay_timestamp;
}

// The SunnyDay, SunnyDayAutoInit, and SunnyDayNearMaxTime test cases directly
// (performs checks in the test case) or indirectly (invokes DoSunnyDay() which
// performs the checks) rely on timing constraints that are prone to failure,
// such as ensuring an action happens within 10 milliseconds. This requirement
// makes the tests flaky since none of these actions can be guaranteed to always
// run within the specified time.

void DoSunnyDay(posix::TakeThenSignalContext* context,
                bool check_timeout,
                bool use_monotonic) {
  pthread_t thread = 0;
  pthread_create(&thread, NULL, posix::TakeThenSignalEntryPoint, context);

  const int64_t kDelayUs = 10'000;  // 10ms
  // Allow two-millisecond-level precision.
  const int64_t kPrecisionUs = 2'000;  // 2ms

  // We know the thread hasn't signaled the condition variable yet, and won't
  // unless we tell it, so it should wait at least the whole delay time.
  if (check_timeout) {
    EXPECT_EQ(pthread_mutex_lock(&context->mutex), 0);
    int64_t start = CurrentTime(use_monotonic);

    struct timespec delay_timestamp =
        CalculateDelayTimestamp(kDelayUs, use_monotonic);

    EXPECT_EQ(pthread_cond_timedwait(&context->condition, &context->mutex,
                                     &delay_timestamp),
              ETIMEDOUT);
    int64_t end = CurrentTime(use_monotonic);
    int64_t elapsed = end - start;

    EXPECT_LE(kDelayUs, elapsed + kPrecisionUs);
    EXPECT_GT(kDelayUs * 2, elapsed - kPrecisionUs);
    EXPECT_EQ(pthread_mutex_unlock(&context->mutex), 0);
  }

  {
    EXPECT_EQ(pthread_mutex_lock(&context->mutex), 0);

    // Tell the thread to signal the condvar, which will cause it to attempt to
    // acquire the mutex we are holding.
    context->do_signal.Put();

    int64_t start = CurrentTime(use_monotonic);

    // We release the mutex when we wait, allowing the thread to actually do the
    // signaling, and ensuring we are waiting before it signals.

    struct timespec delay_timestamp =
        CalculateDelayTimestamp(kDelayUs, use_monotonic);
    EXPECT_EQ(pthread_cond_timedwait(&context->condition, &context->mutex,
                                     &delay_timestamp),
              0);

    // We should have waited only a very small amount of time.
    EXPECT_GT(kDelayUs, CurrentTime(use_monotonic) - start);

    EXPECT_EQ(pthread_mutex_unlock(&context->mutex), 0);
  }

  // Now we wait for the thread to exit.
  EXPECT_TRUE(pthread_join(thread, NULL) == 0);
  EXPECT_EQ(pthread_cond_destroy(&context->condition), 0);
  EXPECT_EQ(pthread_mutex_destroy(&context->mutex), 0);
}

// Test marked as flaky because it calls DoSunnyDay().
TEST(PosixConditionVariableWaitTimedTest, FLAKY_SunnyDay) {
  posix::TakeThenSignalContext context;
  context.delay_after_signal = 0;

  EXPECT_EQ(pthread_mutex_init(&context.mutex, NULL), 0);

#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  InitCondition(&context.condition, true);
  DoSunnyDay(&context, true, true /* use_monotonic */);
#else
  InitCondition(&context.condition, false);
  DoSunnyDay(&context, true, false /* use_monotonic */);
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
}

// Test marked as flaky because it calls DoSunnyDay().
TEST(PosixConditionVariableWaitTimedTest, FLAKY_SunnyDayAutoInit) {
  {
    posix::TakeThenSignalContext context = {posix::TestSemaphore(0),
                                            PTHREAD_MUTEX_INITIALIZER,
                                            PTHREAD_COND_INITIALIZER, 0};

    DoSunnyDay(&context, true, false);
  }

  // Without the initial timeout test, the two threads will be racing to
  // auto-init the mutex and condition variable. So we run several trials in
  // this mode, hoping to have the auto-initting contend in various ways.
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    posix::TakeThenSignalContext context = {posix::TestSemaphore(0),
                                            PTHREAD_MUTEX_INITIALIZER,
                                            PTHREAD_COND_INITIALIZER, 0};
    DoSunnyDay(&context, false, false);
  }
}

// Test marked as flaky because it relies on timing sensitive execution similar
// to DoSunnyDay().
TEST(PosixConditionVariableWaitTimedTest, FLAKY_SunnyDayNearMaxTime) {
  const int64_t kOtherDelaySec = 3'000'000;  // 3s
  posix::TakeThenSignalContext context = {
      posix::TestSemaphore(0), PTHREAD_MUTEX_INITIALIZER,
      PTHREAD_COND_INITIALIZER, kOtherDelaySec};
  EXPECT_EQ(pthread_mutex_init(&context.mutex, NULL), 0);

  InitCondition(&context.condition, false /* use_monotonic */);
  pthread_t thread = 0;
  pthread_create(&thread, NULL, posix::TakeThenSignalEntryPoint, &context);

  EXPECT_EQ(pthread_mutex_lock(&context.mutex), 0);

  // Tell the thread to signal the condvar, which will cause it to attempt to
  // acquire the mutex we are holding, after it waits for delay_after_signal.
  context.do_signal.Put();

  int64_t start = CurrentTime(false /* use_monotonic */);

  // Try to wait until the end of time.
  struct timespec delay_timestamp = {0};
  delay_timestamp.tv_sec = INT_MAX;

  // We release the mutex when we wait, allowing the thread to actually do the
  EXPECT_EQ(pthread_cond_timedwait(&context.condition, &context.mutex,
                                   &delay_timestamp),
            0);

  // We should have waited at least the delay_after_signal amount, but not the
  // full delay.
  int64_t delay_after_singal_sec = context.delay_after_signal / 1'000'000;
  EXPECT_LE(delay_after_singal_sec,
            CurrentTime(false /* use_monotonic */) - start);
  EXPECT_GT(INT_MAX, CurrentTime(false /* use_monotonic */) - start);

  EXPECT_EQ(pthread_mutex_unlock(&context.mutex), 0);

  // Now we wait for the thread to exit.
  EXPECT_TRUE(pthread_join(thread, NULL) == 0);
  EXPECT_EQ(pthread_cond_destroy(&context.condition), 0);
  EXPECT_EQ(pthread_mutex_destroy(&context.mutex), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
