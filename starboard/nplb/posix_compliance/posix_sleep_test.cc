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

constexpr unsigned int kTestSleepSecs = 1;  // 1 second.

// Duration threshold for sleeps that should return immediately.
constexpr long kShortDurationThresholdUs = 100'000;  // 100 milliseconds.
// Duration for sleep(), should be long enough to be interrupted.
constexpr unsigned int kLongSleepSecs = 10;  // 10 seconds.
// Delay in the signal-sending thread before sending the signal.
constexpr long kSignalSendDelayNs = 100'000'000L;

void InterruptSignalHandler(int) {}

void* SendSigusr1Routine(void* arg) {
  pthread_t* target_thread_id = static_cast<pthread_t*>(arg);

  struct timespec delay = {0, kSignalSendDelayNs};
  nanosleep(&delay, nullptr);

  pthread_kill(*target_thread_id, SIGUSR1);

  return nullptr;
}

// Assert that sleep has the signature:
// unsigned int sleep(unsigned int)
static_assert(std::is_same_v<decltype(sleep), unsigned int(unsigned int)>,
              "'sleep' is not declared or does not have the signature "
              "'unsigned int (unsigned int)'");

class PosixSleepTests : public ::testing::Test {
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

TEST_F(PosixSleepTests, SuccessfulSleep) {
  errno = 0;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  unsigned int ret = sleep(kTestSleepSecs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  ASSERT_EQ(0u, ret) << "Expected successful sleep (return 0). Return: " << ret
                     << ", errno: " << errno << " (" << strerror(errno) << ")";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  long requested_us = static_cast<long>(kTestSleepSecs) * 1'000'000L;
  EXPECT_GE(elapsed_us, requested_us)
      << "Sleep duration was too short. Requested: " << requested_us << "us ("
      << kTestSleepSecs << "s), Elapsed: " << elapsed_us << "us.";
}

TEST_F(PosixSleepTests, ZeroDurationSleep) {
  errno = 0;

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  unsigned int ret = sleep(0);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  ASSERT_EQ(0u, ret) << "Expected immediate return for zero duration sleep "
                        "(return 0). Return: "
                     << ret << ", errno: " << errno << " (" << strerror(errno)
                     << ")";

  long elapsed_us = TimevalDiffToMicroseconds(&start_time, &end_time);
  EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
      << "Zero duration sleep took too long. Elapsed: " << elapsed_us
      << "us. Threshold: " << kShortDurationThresholdUs << "us.";
}

TEST_F(PosixSleepTests, ErrorEintrCheckErrnoValue) {
  // TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  RegisterSigusr1Handler();

  pthread_t self_thread_id = pthread_self();
  pthread_t signal_thread_id;

  int create_ret = pthread_create(&signal_thread_id, nullptr,
                                  SendSigusr1Routine, &self_thread_id);
  ASSERT_EQ(0, create_ret) << "Failed to create signal sender thread. Error: "
                           << strerror(create_ret);

  errno = 0;
  unsigned int time_slept_for = kLongSleepSecs;
  sleep(time_slept_for);
  int saved_errno = errno;

  int join_ret = pthread_join(signal_thread_id, nullptr);
  ASSERT_EQ(0, join_ret) << "Failed to join signal sender thread. Error: "
                         << strerror(join_ret);

  EXPECT_EQ(0, saved_errno)
      << "sleep() should not set errno on interruption. Current errno: "
      << saved_errno << " (" << strerror(saved_errno) << ")";
}

TEST_F(PosixSleepTests, ErrorEintrCheckReturnValue) {
  RegisterSigusr1Handler();

  pthread_t self_thread_id = pthread_self();
  pthread_t signal_thread_id;

  int create_ret = pthread_create(&signal_thread_id, nullptr,
                                  SendSigusr1Routine, &self_thread_id);
  ASSERT_EQ(0, create_ret) << "Failed to create signal sender thread. Error: "
                           << strerror(create_ret);

  errno = 0;
  unsigned int time_slept_for = kLongSleepSecs;
  unsigned int ret = sleep(time_slept_for);

  int join_ret = pthread_join(signal_thread_id, nullptr);
  ASSERT_EQ(0, join_ret) << "Failed to join signal sender thread. Error: "
                         << strerror(join_ret);

  EXPECT_NE(0u, ret) << "sleep() should return non-zero (unslept time) when "
                        "interrupted. Got 0.";

  EXPECT_LE(ret, time_slept_for)
      << "Unslept time (" << ret
      << ") should be less than or equal to the requested time ("
      << time_slept_for << ").";

  EXPECT_TRUE(ret == time_slept_for || ret == time_slept_for - 1)
      << "Expected unslept time to be " << time_slept_for << " or "
      << time_slept_for - 1 << ", but got " << ret;
}

}  // namespace
}  // namespace nplb
