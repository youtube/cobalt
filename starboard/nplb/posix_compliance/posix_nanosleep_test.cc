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

#include <cstring>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Small duration for sleeps that should be short or return immediately.
constexpr long kShortSleepNs = 1'000'000L;  // 1 millisecond.
// A slightly longer duration for testing actual sleep.
constexpr long kTestSleepNs = 50'000'000L;  // 50 milliseconds.
constexpr long kTestSleepUs = kTestSleepNs / 1'000;
// A very long duration for testing interruption.
constexpr long kLongSleepSec = 10;  // 10 seconds.
// Duration for alarm to trigger, should be less than kLongSleepSec.
constexpr unsigned int kAlarmSec = 1;
// Duration threshold for sleeps that should return immediately.
constexpr long kShortDurationThresholdUs = 100'000;  // 100 milliseconds.

void InterruptSignalHandler(int) {}

// Assert that nanosleep has the signature:
// int nanosleep(const struct timespec*, struct timespec*)
static_assert(std::is_same_v<decltype(nanosleep),
                             int(const struct timespec*, struct timespec*)>,
              "'nanosleep' is not declared or does not have the signature "
              "'int (const struct timespec*, struct timespec*)'");

class PosixNanosleepTests : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigalrm_;

  void SetUp() override {
    errno = 0;
    alarm(0);

    ASSERT_EQ(sigaction(SIGALRM, nullptr, &old_sa_sigalrm_), 0)
        << "Could not get old SIGALRM handler in SetUp";
  }

  void TearDown() override {
    ASSERT_EQ(sigaction(SIGALRM, &old_sa_sigalrm_, nullptr), 0)
        << "Could not restore old SIGALRM handler in TearDown";
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

TEST_F(PosixNanosleepTests, SuccessfulSleep) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0, kTestSleepNs};
  struct timespec rem;
  errno = 0;
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, kTestSleepUs)
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixNanosleepTests, SuccessfulSleepNullRemain) {
  struct timespec req = {0, kShortSleepNs};
  errno = 0;
  int ret = nanosleep(&req, nullptr);
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain. Return: "
                    << ret << ", errno: " << errno << " (" << strerror(errno)
                    << ")";
}

TEST_F(PosixNanosleepTests, ZeroDurationSleep) {
  struct timespec req = {0, 0L};
  struct timespec rem;
  errno = 0;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = nanosleep(&req, &rem);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  ASSERT_EQ(0, ret)
      << "Expected immediate return for zero duration sleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
      << "Zero duration sleep took too long. Elapsed: " << elapsed_us
      << "us. Threshold: " << kShortDurationThresholdUs << "us.";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1L};
  struct timespec rem;
  errno = 0;
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative ns. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestNsTooLarge) {
  struct timespec req = {0, 1'000'000'000L};
  struct timespec rem;
  errno = 0;
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for ns >= 10^9. errno: " << errno
                           << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0L};
  struct timespec rem;
  errno = 0;
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative tv_sec. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEfaultRequestNull) {
  struct timespec rem;
  errno = 0;

  timespec* req = nullptr;
  int ret = nanosleep(req, &rem);

  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for NULL request pointer.";
  EXPECT_EQ(EFAULT, errno)
      << "Expected EFAULT for NULL request pointer. errno: " << errno << " ("
      << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEintr) {
  struct timespec req = {kLongSleepSec, 0};
  struct timespec rem;

  struct sigaction sa;
  sa.sa_handler = InterruptSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ASSERT_EQ(0, sigaction(SIGALRM, &sa, nullptr));

  alarm(kAlarmSec);

  errno = 0;
  int ret = nanosleep(&req, &rem);
  alarm(0);

  ASSERT_EQ(-1, ret) << "nanosleep should return -1 for NULL request pointer.";
  ASSERT_EQ(EINTR, errno) << "Expected EINTR for interrupted sleep. errno: "
                          << errno << " (" << strerror(errno) << ")";

  // Remaining time is less than requested, or is zero.
  EXPECT_TRUE(rem.tv_sec < req.tv_sec ||
              (rem.tv_sec == req.tv_sec && rem.tv_nsec < req.tv_nsec &&
               rem.tv_nsec >= 0) ||
              (rem.tv_sec == 0 && rem.tv_nsec == 0))
      << "Remaining time sec=" << rem.tv_sec << " nsec=" << rem.tv_nsec
      << " vs requested sec=" << req.tv_sec << " nsec=" << req.tv_nsec;
  EXPECT_GE(rem.tv_sec, 0);
  EXPECT_GE(rem.tv_nsec, 0);
  EXPECT_LT(rem.tv_nsec, 1'000'000'000);
}

}  // namespace
}  // namespace nplb
