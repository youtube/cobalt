// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <limits>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int64_t kMicrosecondsPerSecond = 1'000'000LL;

// kReasonableMinTime represents a time (2025-01-01 00:00:00 UTC) after which
// the current time is expected to fall. In some platforms, time_t is long long
// and in others it's just a long; this type dance below is to guarantee the
// numbers fit into ranges.
const auto kReasonableMinTimeTemp = 1'735'689'600LL;  // 2025-01-01 00:00:00 UTC
static_assert(kReasonableMinTimeTemp >= std::numeric_limits<time_t>::min() &&
                  kReasonableMinTimeTemp <= std::numeric_limits<time_t>::max(),
              "kReasonableMinTimeTemp does not fit in a time_t.");
const time_t kReasonableMinTime = static_cast<time_t>(kReasonableMinTimeTemp);

// kReasonableMaxTime represents a time very far in the future and before which
// the current time is expected to fall. Note that this also implicitly tests
// that the code handles timestamps past the Unix Epoch wraparound on 03:14:08
// UTC on 19 January 2038.
const time_t kReasonableMaxTime = std::numeric_limits<time_t>::max();

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, clock_gettime() is declared "noexcept".
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// Assert that time has the signature:
// time_t time(time_t*)
static_assert(std::is_same_v<decltype(time), time_t(time_t*)>,
              "'time' is not declared or does not have the signature "
              "'time_t (time_t*)'");
#endif

TEST(PosixTimeTimeTests, TimeWithNullArgumentReturnsCurrentTime) {
  time_t current_time = time(nullptr);

  ASSERT_NE(current_time, static_cast<time_t>(-1))
      << "time(nullptr) returned an error: (time_t)-1. errno: " << errno << " "
      << strerror(errno);

  EXPECT_GT(current_time, kReasonableMinTime)
      << "Current time (" << current_time
      << ") is unexpectedly earlier than kReasonableMinTime ("
      << kReasonableMinTime << ").";
}

TEST(PosixTimeTimeTests, TimeWithValidArgumentStoresAndReturnsCurrentTime) {
  time_t time_val_from_arg = 0;
  time_t time_val_returned = time(&time_val_from_arg);

  ASSERT_NE(time_val_returned, static_cast<time_t>(-1))
      << "time(&tloc) returned an error: (time_t)-1. errno: " << errno << " "
      << strerror(errno);

  EXPECT_EQ(time_val_returned, time_val_from_arg)
      << "Returned time and argument time mismatch.";

  EXPECT_GT(time_val_returned, kReasonableMinTime)
      << "Current time (" << time_val_returned
      << ") is unexpectedly earlier than kReasonableMinTime ("
      << kReasonableMinTime << ").";
}

// Comment on EFAULT for time():
// According to POSIX, time() can fail with EFAULT if the tloc argument
// points to an invalid memory address. This error condition is difficult
// to reliably and safely trigger and test in a unit test framework
// without potentially crashing the test runner, as it involves causing
// a segmentation fault or similar memory access violation.

TEST(PosixTimeTimeTests, TimeIsReasonable) {
  time_t now_s = time(nullptr);
  ASSERT_NE(now_s, static_cast<time_t>(-1))
      << "time() returned an error. errno: " << errno << " " << strerror(errno);

  EXPECT_GT(now_s, kReasonableMinTime)
      << "Current time (" << now_s
      << ") is before the expected past threshold (" << kReasonableMinTime
      << ").";

  EXPECT_LT(now_s, kReasonableMaxTime)
      << "Current time (" << now_s
      << ") is after the expected future threshold (" << kReasonableMaxTime
      << ").";
}

TEST(PosixTimeTimeTests,
     ReturnsCurrentTimeAndStoresInArgumentIfNotNullAndIsReasonable) {
  time_t time_val_from_arg = 0;
  time_t time_val_from_return = time(&time_val_from_arg);

  ASSERT_NE(static_cast<time_t>(-1), time_val_from_return)
      << "time() failed with errno: " << errno << " " << strerror(errno);

  EXPECT_EQ(time_val_from_return, time_val_from_arg)
      << "Return value from time() should be identical to the value stored via "
         "non-nullptr argument.";

  EXPECT_GT(time_val_from_return, kReasonableMinTime)
      << "Return value from time() (" << time_val_from_return
      << ") is unexpectedly before kReasonableMinTime (" << kReasonableMinTime
      << ").";
  EXPECT_LT(time_val_from_return, kReasonableMaxTime)
      << "Return value from time() (" << time_val_from_return
      << ") is unexpectedly after kReasonableMaxTime (" << kReasonableMaxTime
      << ").";
}

TEST(PosixTimeTimeTests, MatchesGettimeofdayWithNonNullArgument) {
  time_t time_val_from_arg = 0;
  struct timeval current_timeval;

  ASSERT_NE(-1, gettimeofday(&current_timeval, nullptr))
      << "gettimeofday() failed with errno: " << errno << " "
      << strerror(errno);

  time_t time_val_from_return = time(&time_val_from_arg);

  ASSERT_NE(static_cast<time_t>(-1), time_val_from_return)
      << "time() failed with errno: " << errno << " " << strerror(errno);

  int64_t time_func_us =
      static_cast<int64_t>(time_val_from_return) * kMicrosecondsPerSecond;
  int64_t gettimeofday_func_us =
      (static_cast<int64_t>(current_timeval.tv_sec) * kMicrosecondsPerSecond) +
      current_timeval.tv_usec;

  // The time reported by time() should be very close to gettimeofday().
  // A tolerance of one second (kMicrosecondsPerSecond) accounts for potential
  // rollover of the second between the two calls.
  EXPECT_NEAR(time_func_us, gettimeofday_func_us, kMicrosecondsPerSecond)
      << "time() result (" << time_val_from_return << "s) "
      << "and gettimeofday() result (" << current_timeval.tv_sec << "s "
      << current_timeval.tv_usec << "µs) "
      << "differ by more than the allowed threshold of one second.";
}

TEST(PosixTimeTimeTests, MatchesGettimeofdayWithNullArgument) {
  struct timeval current_timeval;

  ASSERT_NE(-1, gettimeofday(&current_timeval, nullptr))
      << "gettimeofday() failed with errno: " << errno << " "
      << strerror(errno);

  time_t time_val_from_return = time(nullptr);

  ASSERT_NE(static_cast<time_t>(-1), time_val_from_return)
      << "time() failed with errno: " << errno << " " << strerror(errno);

  int64_t time_func_us =
      static_cast<int64_t>(time_val_from_return) * kMicrosecondsPerSecond;
  int64_t gettimeofday_func_us =
      (static_cast<int64_t>(current_timeval.tv_sec) * kMicrosecondsPerSecond) +
      current_timeval.tv_usec;

  EXPECT_NEAR(time_func_us, gettimeofday_func_us, kMicrosecondsPerSecond)
      << "time(nullptr) result (" << time_val_from_return << "s) "
      << "and gettimeofday() result (" << current_timeval.tv_sec << "s "
      << current_timeval.tv_usec << "µs) "
      << "differ by more than the allowed threshold of one second.";
}

TEST(PosixTimeTimeTests, TimeProgressesMonotonically) {
  const int kNumIterations = 1000;
  for (int i = 0; i < kNumIterations; ++i) {
    time_t time1 = time(nullptr);
    ASSERT_NE(time1, static_cast<time_t>(-1))
        << "First call to time(nullptr) failed. errno: " << errno << " "
        << strerror(errno);

    time_t time2 = time(nullptr);
    ASSERT_NE(time2, static_cast<time_t>(-1))
        << "Second call to time(nullptr) failed. errno: " << errno << " "
        << strerror(errno);

    EXPECT_GE(time2, time1)
        << "Time is expected to be monotonically non-decreasing. Time 1: "
        << time1 << ", Time 2: " << time2;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
