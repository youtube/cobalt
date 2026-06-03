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
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// A slightly longer duration for testing actual sleep.
const useconds_t kTestSleepUs =
    50'000;  // 50 milliseconds (50,000 microseconds)
// A very long duration for testing interruption.
const useconds_t kLongSleepUs = 10'000'000;  // 10 seconds
// Delay in the signal-sending thread before sending the signal.
constexpr long kSignalSendDelayNs = 100'000'000L;

// Duration threshold for sleeps that should return immediately.
constexpr long kShortDurationThresholdUs = 100'000;  // 100 milliseconds.

void InterruptSignalHandler(int) {}

void* SendSigusr1Routine(void* arg) {
  pthread_t* target_thread_id = static_cast<pthread_t*>(arg);

  struct timespec delay = {0, kSignalSendDelayNs};
  nanosleep(&delay, nullptr);

  pthread_kill(*target_thread_id, SIGUSR1);

  return nullptr;
}

// Assert that usleep has the signature:
// int usleep(useconds_t)
static_assert(std::is_same_v<decltype(usleep), int(useconds_t)>,
              "'usleep' is not declared or does not have the signature "
              "'int (useconds_t)'");

class PosixUsleepTests : public ::testing::Test {
 protected:
  struct sigaction old_sa_sigusr1_;
  sigset_t original_mask_;
  bool sigusr1_handler_set_ = false;

  void SetUp() override {
    // Make sure the signal is unblocked.
    sigset_t unblock_mask;
    sigemptyset(&unblock_mask);
    sigaddset(&unblock_mask, SIGUSR1);

    ASSERT_EQ(sigprocmask(SIG_UNBLOCK, &unblock_mask, &original_mask_), 0);

    errno = 0;
    ASSERT_EQ(sigaction(SIGUSR1, nullptr, &old_sa_sigusr1_), 0)
        << "Could not get old SIGUSR1 handler in SetUp";
  }

  void TearDown() override {
    if (sigusr1_handler_set_) {
      ASSERT_EQ(sigaction(SIGUSR1, &old_sa_sigusr1_, nullptr), 0)
          << "Could not restore old SIGUSR1 handler in TearDown";
    }

    // Restore the original mask.
    ASSERT_EQ(sigprocmask(SIG_SETMASK, &original_mask_, nullptr), 0);
  }

  void RegisterSigusr1Handler() {
    struct sigaction sa;
    sa.sa_handler = InterruptSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ASSERT_EQ(0, sigaction(SIGUSR1, &sa, nullptr))
        << "Failed to set SIGUSR1 handler. Errno: " << errno << " ("
        << strerror(errno) << ")";
    sigusr1_handler_set_ = true;
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

TEST_F(PosixUsleepTests, SuccessfulSleep) {
  errno = 0;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = usleep(kTestSleepUs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  ASSERT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixUsleepTests, ZeroDurationSleep) {
  errno = 0;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = usleep(0);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  ASSERT_EQ(0, ret)
      << "Expected immediate return for zero duration usleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
      << "Zero duration usleep took too long. Elapsed: " << elapsed_us
      << "us. Threshold: " << kShortDurationThresholdUs << "us.";
}

TEST_F(PosixUsleepTests, ErrorEintr) {
  RegisterSigusr1Handler();

  pthread_t self_thread_id = pthread_self();
  pthread_t signal_thread_id;

  int create_ret = pthread_create(&signal_thread_id, nullptr,
                                  SendSigusr1Routine, &self_thread_id);
  ASSERT_EQ(0, create_ret) << "Failed to create signal sender thread. Error: "
                           << strerror(create_ret);

  errno = 0;
  int ret = usleep(kLongSleepUs);

  int join_ret = pthread_join(signal_thread_id, nullptr);
  ASSERT_EQ(0, join_ret) << "Failed to join signal sender thread. Error: "
                         << strerror(join_ret);

  EXPECT_EQ(-1, ret) << "usleep should return -1 when interrupted.";
  EXPECT_EQ(EINTR, errno) << "Expected EINTR for interrupted sleep. Got errno: "
                          << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
