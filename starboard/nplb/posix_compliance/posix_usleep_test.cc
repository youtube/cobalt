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
#include <unistd.h>    // For usleep()
#include <cstring>     // For strerror
#include <ctime>       // For timespec, nanosleep (for delay in thread)

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Small duration for sleeps that should be short or return immediately.
const useconds_t kShortSleepUs = 1000;  // 1 millisecond (1,000 microseconds)
// A slightly longer duration for testing actual sleep.
const useconds_t kTestSleepUs = 50000;  // 50 milliseconds (50,000 microseconds)
// A very long duration for testing interruption.
const useconds_t kLongSleepUs = 10000000;  // 10 seconds

// Delay in the signal-sending thread before sending the signal.
// This gives usleep a chance to start. 100 milliseconds.
const long kSignalSendDelayNs = 100000000L;

// Signal handler for interruption tests
void InterruptSignalHandler(int signum) {
  // This handler's purpose is solely to interrupt a syscall.
  // It's intentionally empty.
  (void)signum;  // Suppress unused parameter warning
}

// Arguments for the signal-sending thread
struct SignalThreadArgs {
  pthread_t target_thread_id;
  // You could add a delay here if you want it configurable per call
  // For now, kSignalSendDelayNs is a global constant.
};

// Thread function to send SIGUSR1
void* SendSigusr1Routine(void* arg) {
  SignalThreadArgs* args = static_cast<SignalThreadArgs*>(arg);

  // Give a brief moment for usleep to start in the main thread
  struct timespec delay = {0, kSignalSendDelayNs};
  nanosleep(&delay, nullptr);  // Ignoring return value of nanosleep for
                               // simplicity in test utility

  // Send SIGUSR1 to the target (main) thread
  pthread_kill(args->target_thread_id, SIGUSR1);

  return nullptr;
}

// Define the test fixture if it's not already defined elsewhere
// Assumes a similar setup as PosixClockNanosleepTest for handler restoration
class PosixUsleepTests : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigusr1;
  bool sigusr1_handler_set = false;

  void SetUp() override {
    errno = 0;  // Clear errno before each test
    // Store current SIGUSR1 handler to restore it later
    if (sigaction(SIGUSR1, nullptr, &old_sa_sigusr1) != 0) {
      // This might happen if SIGUSR1 is blocked or invalid, though unlikely.
      // We'll proceed but TearDown might not restore perfectly.
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
    sa.sa_flags = 0;  // No SA_RESTART, to ensure EINTR for syscalls like usleep
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

// Test suite for POSIX usleep() function.
// The tests are named using the TEST macro, with PosixUsleepTests
// as the test suite name.
// Test suite for POSIX usleep() function.
// The tests are named using the TEST macro, with PosixUsleepTests
// as the test suite name.
TEST_F(PosixUsleepTests, SuccessfulSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = usleep(kTestSleepUs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  EXPECT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    // Check that the elapsed time is at least the requested sleep time.
    // Due to system scheduling, it might sleep slightly longer.
    EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
        << "Sleep duration was too short. Requested: " << kTestSleepUs
        << "us, Elapsed: " << elapsed_us << "us.";
  }
}

TEST_F(PosixUsleepTests, ZeroDurationSleep) {
  errno = 0;            // Clear errno before the call
  int ret = usleep(0);  // Request: 0 microseconds
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration sleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test EINTR for usleep using a pthread to send SIGUSR1
TEST_F(PosixUsleepTests, ErrorEintrRelativeSleep) {
  RegisterSigusr1Handler();  // Set up the SIGUSR1 handler

  pthread_t self_thread_id = pthread_self();
  SignalThreadArgs thread_args = {self_thread_id};
  pthread_t signal_thread_id;

  // Create the thread that will send SIGUSR1
  int create_ret = pthread_create(&signal_thread_id, nullptr,
                                  SendSigusr1Routine, &thread_args);
  ASSERT_EQ(0, create_ret) << "Failed to create signal sender thread. Error: "
                           << strerror(create_ret);

  errno = 0;  // Clear errno right before the call to usleep
  int ret = usleep(kLongSleepUs);

  // Wait for the signal-sending thread to complete its work
  int join_ret = pthread_join(signal_thread_id, nullptr);
  ASSERT_EQ(0, join_ret) << "Failed to join signal sender thread. Error: "
                         << strerror(join_ret);

  // usleep returns -1 on error and sets errno.
  // One common error is interruption by a signal (EINTR).
  EXPECT_EQ(-1, ret) << "usleep should return -1 when interrupted.";
  EXPECT_EQ(EINTR, errno) << "Expected EINTR for interrupted sleep. Got errno: "
                          << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
