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
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Small duration for sleeps that should be short or return immediately.
const long kShortSleepNs = 1'000'000;  // 1 millisecond.
const long kShortSleepUs = kShortSleepNs / 1'000;
// A slightly longer duration for testing actual sleep.
const long kTestSleepNs = 50'000'000;  // 50 milliseconds.
const long kTestSleepUs = kTestSleepNs / 1'000;
// A very long duration for testing interruption.
const long kLongSleepSec = 10;  // 10 seconds.
// Duration for alarm to trigger, should be less than kLongSleepSec.
const unsigned int kAlarmSec = 1;
// Duration threshold for sleeps that should return immediately.
const long kShortDurationThresholdUs = 100000;  // 100 milliseconds.

void InterruptSignalHandler(int signum) {
  (void)signum;  // Suppress unused parameter warning.
}

class PosixClockNanosleepTest : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigalrm;

  void SetUp() override {
    errno = 0;
    alarm(0);

    if (sigaction(SIGALRM, nullptr, &old_sa_sigalrm) != 0) {
      perror("Warning: Could not get old SIGALRM handler in SetUp");
    }
  }

  void TearDown() override {
    if (sigaction(SIGALRM, &old_sa_sigalrm, nullptr) != 0) {
      perror("Warning: Could not restore old SIGALRM handler in TearDown");
    }
    alarm(0);
  }
};

long TimevalDiffToMicroseconds(const struct timeval* start,
                               const struct timeval* end) {
  if (!start || !end) {
    return 0;
  }
  long seconds_diff = end->tv_sec - start->tv_sec;
  long useconds_diff = end->tv_usec - start->tv_usec;
  return (seconds_diff * 1'000'000L) + useconds_diff;
}

TEST_F(PosixClockNanosleepTest, RelativeSleepMonotonicClock) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0, kTestSleepNs};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixClockNanosleepTest, RelativeSleepRealtimeClock) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0, kTestSleepNs};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_REALTIME, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixClockNanosleepTest, AbsoluteSleepMonotonicClock) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1'000'000'000) {
    req.tv_sec++;
    req.tv_nsec -= 1'000'000'000;
  }

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixClockNanosleepTest, AbsoluteSleepRealtimeClock) {
  struct timespec req;
  clockid_t clock_id = CLOCK_REALTIME;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_REALTIME";

  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1'000'000'000) {
    req.tv_sec++;
    req.tv_nsec -= 1'000'000'000;
  }

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixClockNanosleepTest, AbsoluteSleepTimeInPastReturnsImmediately) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;
  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  if (req.tv_nsec >= kShortSleepNs) {
    req.tv_nsec -= kShortSleepNs;
  } else if (req.tv_sec > 0) {
    req.tv_sec--;
    req.tv_nsec += (1'000'000'000 - kShortSleepNs);
  } else {
    // Clock is too close to zero to subtract kShortSleepNs, just set to 0.
    req.tv_sec = 0;
    req.tv_nsec = 0;
  }

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected immediate return for past absolute time, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_LT(elapsed_us, static_cast<long>(kShortSleepUs))
      << "Sleep duration was too long. Requested time in the past, Elapsed: "
      << elapsed_us << "us.";
}

TEST_F(PosixClockNanosleepTest, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for negative ns, got: "
                         << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, ErrorEinvalRequestNsTooLarge) {
  struct timespec req = {0, 1000000000};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for ns >= 10^9, got: "
                         << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret)
      << "Expected EINVAL for negative tv_sec in relative sleep, got: "
      << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, ErrorEinvalInvalidClockId) {
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  clockid_t bad_clock_id = -123;  // An unlikely to be valid clock ID
  int ret = clock_nanosleep(bad_clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid clock_id, got: "
                         << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, ErrorEinvalInvalidFlags) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  int invalid_flags = 0xFF;  // Some likely invalid flags
  int ret = clock_nanosleep(CLOCK_MONOTONIC, invalid_flags, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid flags, got: "
                         << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, ErrorEnotsupForThreadCpuClock) {
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_THREAD_CPUTIME_ID, 0, &req, &rem);

  // Some systems might return EINVAL if CLOCK_THREAD_CPUTIME_ID is not a
  // "known clock" in this context, or ENOTSUP if it's known but not usable for
  // sleep. Since a thread should not be using CPU time during sleep,
  // CLOCK_THREAD_CPUTIME_ID should be invalid or not supported.
  EXPECT_TRUE(ret == ENOTSUP || ret == EINVAL)
      << "Expected ENOTSUP or EINVAL for CLOCK_THREAD_CPUTIME_ID, got: "
      << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, RelativeSleepNullRemain) {
  struct timespec req = {0, kShortSleepNs};  // 1 millisecond
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, nullptr);
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain, got: "
                    << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, AbsoluteSleepNullRemain) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;
  ASSERT_EQ(0, clock_gettime(clock_id, &req));
  req.tv_nsec += kShortSleepNs;
  if (req.tv_nsec >= 1'000'000'000) {
    req.tv_sec++;
    req.tv_nsec -= 1'000'000'000;
  }
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, nullptr);
  EXPECT_EQ(0, ret)
      << "Expected successful absolute sleep with NULL remain, got: "
      << strerror(ret);
}

TEST_F(PosixClockNanosleepTest, RelativeSleepZeroDuration) {
  struct timespec req = {0, 0};
  struct timespec rem;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration relative sleep, got: "
      << strerror(ret);

  if (ret == 0) {
    long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
    EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
        << "Zero duration sleep took too long. Elapsed: " << elapsed_us
        << "us. Threshold: " << kShortDurationThresholdUs << "us.";
  }
}

TEST_F(PosixClockNanosleepTest, ErrorEintrRelativeSleep) {
  struct timespec req = {kLongSleepSec, 0};
  struct timespec rem;

  struct sigaction sa;
  sa.sa_handler = InterruptSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ASSERT_EQ(0, sigaction(SIGALRM, &sa, nullptr));

  alarm(kAlarmSec);

  errno = 0;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  alarm(0);

  EXPECT_EQ(EINTR, ret) << "Expected EINTR, got: " << ret << " (errno was "
                        << errno << " - " << strerror(errno) << ")";
  if (ret == EINTR) {
    EXPECT_TRUE(rem.tv_sec < req.tv_sec ||
                (rem.tv_sec == req.tv_sec && rem.tv_nsec < req.tv_nsec &&
                 rem.tv_nsec >= 0) ||
                (rem.tv_sec == 0 && rem.tv_nsec == 0))
        << "Remaining time sec=" << rem.tv_sec << " nsec=" << rem.tv_nsec
        << " vs requested sec=" << req.tv_sec << " nsec=" << req.tv_nsec;
    EXPECT_GE(rem.tv_sec, 0);
    EXPECT_GE(rem.tv_nsec, 0);
    EXPECT_LT(rem.tv_nsec, 1000000000);
  }
}

TEST_F(PosixClockNanosleepTest, ErrorEintrAbsoluteSleep) {
  struct timespec req;
  struct timespec rem = {7, 7};
  clockid_t clock_id = CLOCK_MONOTONIC;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC, errno: " << errno
      << " (" << strerror(errno) << ")";
  req.tv_sec += kLongSleepSec;

  struct sigaction sa;
  sa.sa_handler = InterruptSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ASSERT_EQ(0, sigaction(SIGALRM, &sa, nullptr));

  alarm(kAlarmSec);

  errno = 0;
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  alarm(0);

  EXPECT_EQ(EINTR, ret) << "Expected EINTR for absolute sleep, got: " << ret
                        << " (errno was " << errno << " - " << strerror(errno)
                        << ")";
  EXPECT_EQ(7, rem.tv_sec) << "rem.tv_sec should be unmodified";
  EXPECT_EQ(7, rem.tv_nsec) << "rem.tv_nsec should be unmodified";
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
