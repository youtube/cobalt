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
#include <sys/time.h>  // For gettimeofday()
#include <time.h>
#include <cstring>  // For strerror

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Small duration for sleeps that should be short or return immediately.
const long kShortSleepNs = 1'000'000L;  // 1 millisecond
// A slightly longer duration for testing actual sleep.
const long kTestSleepNs = 50'000'000L;  // 50 milliseconds
const long kTestSleepUs = kTestSleepNs / 1'000;
// A very long duration for testing interruption.
const long kLongSleepSec = 10;  // 10 seconds
// Duration for alarm to trigger, should be less than kLongSleepSec
const unsigned int kAlarmSec = 1;

// Signal handler for EINTR tests (can be used for SIGALRM)
void InterruptSignalHandler(int signum) {
  // Do nothing, just here to interrupt sleep
  (void)signum;  // Suppress unused parameter warning
}

class PosixNanosleepTests : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigalrm;  // To store original SIGALRM handler

  void SetUp() override {
    // Ensure no stale errno
    errno = 0;
    // Clear any pending alarm from a previous failed test or other source
    alarm(0);

    // Store current SIGALRM handler
    struct sigaction current_sa;
    if (sigaction(SIGALRM, nullptr, &old_sa_sigalrm) != 0) {
      // This should ideally not happen in a test setup.
      // If it does, we might not be able to restore it properly.
      perror("Warning: Could not get old SIGALRM handler in SetUp");
    }
  }

  void TearDown() override {
    // Restore default/original signal handler for SIGALRM
    if (sigaction(SIGALRM, &old_sa_sigalrm, nullptr) != 0) {
      perror("Warning: Could not restore old SIGALRM handler in TearDown");
    }
    // Clear any pending alarm
    alarm(0);
  }
};

// Helper function to calculate the difference between two timeval structs in
// microseconds.
long timeval_diff_us(const struct timeval* start, const struct timeval* end) {
  if (!start || !end) {
    return 0;
  }
  long seconds_diff = end->tv_sec - start->tv_sec;
  long useconds_diff = end->tv_usec - start->tv_usec;
  return (seconds_diff * 1000000L) + useconds_diff;
}

// Test suite for POSIX nanosleep() function.
// The tests are named using the TEST macro, with PosixNanosleepTests
// as the test suite name.
TEST_F(PosixNanosleepTests, SuccessfulSleep) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0,
                         kTestSleepNs};  // Request: 0 seconds, 50 milliseconds
  struct timespec rem;  // To store remaining time, if any (unused on success)
  errno = 0;            // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";
  // POSIX states that if nanosleep returns 0, the contents of 'rem' are
  // unspecified.

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixNanosleepTests, SuccessfulSleepNullRemain) {
  struct timespec req = {0,
                         kShortSleepNs};  // Request: 0 seconds, 1 millisecond
  errno = 0;                              // Clear errno before the call
  int ret = nanosleep(&req, nullptr);     // 'rem' argument is NULL
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain. Return: "
                    << ret << ", errno: " << errno << " (" << strerror(errno)
                    << ")";
}

TEST_F(PosixNanosleepTests, ZeroDurationSleep) {
  struct timespec req = {0, 0L};  // Request: 0 seconds, 0 nanoseconds
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration sleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1L};  // Invalid: nanoseconds part is negative
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative ns. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestNsTooLarge) {
  // Invalid: nanoseconds part is >= 1,000,000,000
  struct timespec req = {0, 1'000'000'000L};
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for ns >= 10^9. errno: " << errno
                           << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0L};  // Invalid: seconds part is negative
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative tv_sec. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixNanosleepTests, ErrorEfaultRequestNull) {
  struct timespec rem;  // 'rem' can be valid or NULL for this specific test.
  errno = 0;            // Clear errno before the call

  timespec* req = nullptr;
  int ret = nanosleep(req, &rem);

  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for NULL request pointer.";
  EXPECT_EQ(EFAULT, errno)
      << "Expected EFAULT for NULL request pointer. errno: " << errno << " ("
      << strerror(errno) << ")";
}

// Test EINTR for relative sleep using SIGALRM
TEST_F(PosixNanosleepTests, ErrorEintrRelativeSleep) {
  struct timespec req = {kLongSleepSec, 0};  // Sleep longer than alarm
  struct timespec rem;

  struct sigaction sa;
  sa.sa_handler = InterruptSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  // No SA_RESTART, to ensure EINTR
  ASSERT_EQ(0, sigaction(SIGALRM, &sa, nullptr));

  alarm(kAlarmSec);  // Send SIGALRM after kAlarmSec seconds

  errno = 0;
  int ret = nanosleep(&req, &rem);
  alarm(0);  // Cancel any pending alarm immediately after sleep returns

  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for NULL request pointer.";
  EXPECT_EQ(EINTR, errno) << "Expected EINTR for interrupted sleep. errno: "
                          << errno << " (" << strerror(errno) << ")";
  if (ret == -1 && errno == EINTR) {
    // Check if 'rem' contains a plausible remaining time
    EXPECT_TRUE(
        rem.tv_sec < req.tv_sec ||
        (rem.tv_sec == req.tv_sec && rem.tv_nsec < req.tv_nsec &&
         rem.tv_nsec >= 0) ||
        (rem.tv_sec == 0 && rem.tv_nsec == 0)  // Interrupted very quickly
        )
        << "Remaining time sec=" << rem.tv_sec << " nsec=" << rem.tv_nsec
        << " vs requested sec=" << req.tv_sec << " nsec=" << req.tv_nsec;
    EXPECT_GE(rem.tv_sec, 0);
    EXPECT_GE(rem.tv_nsec, 0);
    EXPECT_LT(rem.tv_nsec, 1000000000);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
