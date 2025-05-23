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
#include <unistd.h>    // For sleep()
#include <cstring>     // For strerror
#include <ctime>       // For timespec, nanosleep (for delay in thread)

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Duration for testing actual sleep.
const unsigned int kTestSleepSecs = 1;  // 1 second
// A very short duration in microseconds, for checking if ZeroDurationSleep is
// fast.
const long kShortDurationThresholdUs = 100000;  // 100 milliseconds

// Duration for sleep(), should be long enough to be interrupted.
const unsigned int kLongSleepSecs = 10;  // 10 seconds

// Delay in the signal-sending thread before sending the signal.
// This gives sleep() a chance to start. 100 milliseconds.
const long kSignalSendDelayNs = 100000000L;

// Signal handler for interruption tests (can be reused)
void InterruptSignalHandler(int signum) {
  // This handler's purpose is solely to interrupt a syscall.
  // It's intentionally empty.
  (void)signum;  // Suppress unused parameter warning
}

// Arguments for the signal-sending thread (can be reused)
struct SignalThreadArgs {
  pthread_t target_thread_id;
};

// Thread function to send SIGUSR1 (can be reused)
void* SendSigusr1Routine(void* arg) {
  SignalThreadArgs* args = static_cast<SignalThreadArgs*>(arg);

  // Give a brief moment for sleep() to start in the main thread
  struct timespec delay = {0, kSignalSendDelayNs};
  nanosleep(&delay, nullptr);  // Ignoring return value for simplicity

  // Send SIGUSR1 to the target (main) thread
  pthread_kill(args->target_thread_id, SIGUSR1);

  return nullptr;
}

// Define a test fixture for sleep() tests
class PosixSleepTests : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigusr1;
  bool sigusr1_handler_set = false;

  void SetUp() override {
    errno = 0;  // Clear errno before each test
    // Store current SIGUSR1 handler to restore it later
    if (sigaction(SIGUSR1, nullptr, &old_sa_sigusr1) != 0) {
      perror("Warning: Could not get current SIGUSR1 handler in SetUp");
    }
  }

  void TearDown() override {
    // Restore the original SIGUSR1 signal handler if we changed it.
    if (sigusr1_handler_set) {
      if (sigaction(SIGUSR1, &old_sa_sigusr1, nullptr) != 0) {
        perror(
            "Warning: Could not restore original SIGUSR1 handler in TearDown");
      }
    }
  }

  void RegisterSigusr1Handler() {
    struct sigaction sa;
    sa.sa_handler = InterruptSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags =
        0;  // No SA_RESTART, to ensure EINTR for syscalls like sleep()
    ASSERT_EQ(0, sigaction(SIGUSR1, &sa, nullptr))
        << "Failed to set SIGUSR1 handler. Errno: " << errno << " ("
        << strerror(errno) << ")";
    sigusr1_handler_set = true;
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

// Note on error conditions not tested:
// - EINTR: Not tested here because Starboard (as per the original example's
//   context) does not implement full signal handling that would typically lead
//   to EINTR for sleep functions. If EINTR were to occur, sleep() would
//   return the number of unslept seconds.

// Test suite for POSIX sleep() function.
// The tests are named using the TEST macro, with PosixSleepTests
// as the test suite name.
TEST_F(PosixSleepTests, SuccessfulSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  // The sleep() function returns 0 on success, or the number of unslept
  // seconds if interrupted by a signal.
  unsigned int ret = sleep(kTestSleepSecs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  EXPECT_EQ(0, ret) << "Expected successful sleep (return 0). Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    long requested_us = static_cast<long>(kTestSleepSecs) * 1000000L;
    // Check that the elapsed time is at least the requested sleep time.
    // Due to system scheduling, it might sleep slightly longer.
    EXPECT_GE(elapsed_us, requested_us)
        << "Sleep duration was too short. Requested: " << requested_us << "us ("
        << kTestSleepSecs << "s), Elapsed: " << elapsed_us << "us.";
  }
}

TEST_F(PosixSleepTests, ZeroDurationSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  unsigned int ret = sleep(0);  // Request: 0 seconds

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  // sleep(0) should return 0 if successful.
  EXPECT_EQ(0, ret) << "Expected immediate return for zero duration sleep "
                       "(return 0). Return: "
                    << ret << ", errno: " << errno << " (" << strerror(errno)
                    << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    // For a zero duration sleep, the elapsed time should be very small.
    EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
        << "Zero duration sleep took too long. Elapsed: " << elapsed_us
        << "us. Threshold: " << kShortDurationThresholdUs << "us.";
  }
}

// Test EINTR behavior for sleep() using a pthread to send SIGUSR1
TEST_F(PosixSleepTests, ErrorEintrCheckReturnValue) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  RegisterSigusr1Handler();  // Set up the SIGUSR1 handler

  pthread_t self_thread_id = pthread_self();
  SignalThreadArgs thread_args = {self_thread_id};
  pthread_t signal_thread_id;

  // Create the thread that will send SIGUSR1
  int create_ret = pthread_create(&signal_thread_id, nullptr,
                                  SendSigusr1Routine, &thread_args);
  ASSERT_EQ(0, create_ret) << "Failed to create signal sender thread. Error: "
                           << strerror(create_ret);

  errno =
      0;  // Clear errno, though sleep() itself shouldn't set it on interrupt.
  unsigned int time_slept_for =
      kLongSleepSecs;  // Initialize with original duration
  unsigned int ret = sleep(time_slept_for);

  // Wait for the signal-sending thread to complete its work
  int join_ret = pthread_join(signal_thread_id, nullptr);
  ASSERT_EQ(0, join_ret) << "Failed to join signal sender thread. Error: "
                         << strerror(join_ret);

  // According to POSIX:
  // If sleep() is interrupted by a signal and returns, the return value shall
  // be the "unslept" amount (the requested time minus the time actually slept)
  // in seconds. If the requested time has elapsed, the value returned shall be
  // 0. The sleep() function does not set errno.

  // We expect the sleep to be interrupted, so `ret` should be non-zero.
  EXPECT_NE(0u, ret) << "sleep() should return non-zero (unslept time) when "
                        "interrupted. Got 0.";

  // The returned value (unslept time) should be less than or equal to the
  // original requested time.
  EXPECT_LE(ret, time_slept_for)
      << "Unslept time (" << ret
      << ") should be less than or equal to the requested time ("
      << time_slept_for << ").";

  // Given that the signal is sent very quickly (after kSignalSendDelayNs),
  // sleep() likely hasn't completed a full second.
  // Thus, `ret` should be `time_slept_for` or `time_slept_for - 1` (due to
  // sleep's 1-second resolution). A common behavior is for `ret` to be
  // `time_slept_for` if interrupted before the first "tick".
  EXPECT_TRUE(ret == time_slept_for || ret == time_slept_for - 1)
      << "Expected unslept time to be " << time_slept_for << " or "
      << time_slept_for - 1 << ", but got " << ret;

  // Verify that errno is not set to EINTR (or any other error by sleep()
  // itself) Note: Other things could potentially set errno between sleep()
  // returning and this check, but sleep() itself should not set it. This
  // assertion might be fragile if other operations in the test support
  // libraries set errno. For a strict test of sleep() behavior, one might save
  // errno immediately after sleep returns.
  int saved_errno = errno;
  EXPECT_EQ(0, saved_errno)
      << "sleep() should not set errno on interruption. Current errno: "
      << saved_errno << " (" << strerror(saved_errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
