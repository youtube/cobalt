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
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <atomic>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

enum ClockIsRequired { REQUIRED, OPTIONAL };

const int64_t kMicrosecondsPerSecond = 1'000'000LL;

// Sleep time long enough to ensure measurable time progresses on all wall
// clocks.
constexpr long kSleepDurationMicroseconds = 100'000;  // 100ms
constexpr struct timespec kSleepDuration = {
    0, kSleepDurationMicroseconds * 1'000L};

// Work time long enough to ensure measurable CPU time usage.
constexpr long kMinimumWorkTimeMicroseconds = 100'000;  // 100ms

static int64_t TimespecToMicroseconds(const struct timespec& ts) {
  return (static_cast<int64_t>(ts.tv_sec) * kMicrosecondsPerSecond) +
         (static_cast<int64_t>(ts.tv_nsec) / 1'000LL);
}

static std::string GetClockName(clockid_t clock_id) {
  switch (clock_id) {
    case CLOCK_REALTIME:
      return "CLOCK_REALTIME";
    case CLOCK_MONOTONIC:
      return "CLOCK_MONOTONIC";
    case CLOCK_PROCESS_CPUTIME_ID:
      return "CLOCK_PROCESS_CPUTIME_ID";
    case CLOCK_THREAD_CPUTIME_ID:
      return "CLOCK_THREAD_CPUTIME_ID";
    case CLOCK_MONOTONIC_RAW:
      return "CLOCK_MONOTONIC_RAW";
    case CLOCK_REALTIME_COARSE:
      return "CLOCK_REALTIME_COARSE";
    case CLOCK_MONOTONIC_COARSE:
      return "CLOCK_MONOTONIC_COARSE";
    case CLOCK_BOOTTIME:
      return "CLOCK_BOOTTIME";
    case CLOCK_REALTIME_ALARM:
      return "CLOCK_REALTIME_ALARM";
    case CLOCK_BOOTTIME_ALARM:
      return "CLOCK_BOOTTIME_ALARM";
    case CLOCK_TAI:
      return "CLOCK_TAI";
    default:
      // Return a gtest-friendly name for unknown clock IDs
      return "UnknownClockId_" + std::to_string(clock_id);
  }
}

// Helper function to do CPU work for until at least work_time_microseconds
// elapsed on the monotonic wall clock. Return the elapsed time in microseconds.
// Note: This function uses CLOCK_MONOTONIC to measure the elapsed
// time, not the CPU time clock. This is intentional, as the goal is to
// consume CPU time for a specific duration as measured by wall clock time.
// The CPU time clock is then checked to see how much CPU time was actually
// consumed.
long ConsumeCpuForDuration(long work_time_microseconds) {
  struct timespec ts_before{};
  int ret_before = clock_gettime(CLOCK_MONOTONIC, &ts_before);
  // Note: Testing using EXPECT_EQ because ASSERT_EQ can not return with a
  // value.
  EXPECT_EQ(0, ret_before);
  if (ret_before) {
    return 0;
  }
  int64_t time_before_us = TimespecToMicroseconds(ts_before);
  int64_t time_after_us = 0;
  // Loop for at least 100ms to ensure some measurable CPU time is consumed.
  // Using volatile to prevent compiler from optimizing away the loop.
  volatile int_fast32_t counter_sum = 0;
  do {
    const int kCpuWorkIterations = 2'000'000;
    for (int i = 0; i < kCpuWorkIterations; ++i) {
      counter_sum += i;  // Simple work
    }
    struct timespec ts_after{};
    int ret_current = clock_gettime(CLOCK_MONOTONIC, &ts_after);
    // Note: Testing using EXPECT_EQ because ASSERT_EQ can not return with a
    // value.
    EXPECT_EQ(0, ret_current);
    if (ret_current) {
      return 0;
    }
    time_after_us = TimespecToMicroseconds(ts_after);
  } while ((time_after_us - time_before_us) < work_time_microseconds);
  return time_after_us - time_before_us;
}

// A helper template to check for the existence of tv_sec
template <typename T, typename = void>
struct has_tv_sec : std::false_type {};

template <typename T>
struct has_tv_sec<T, std::void_t<decltype(T::tv_sec)>> : std::true_type {};

// A helper template to check for the existence of tv_nsec
template <typename T, typename = void>
struct has_tv_nsec : std::false_type {};

template <typename T>
struct has_tv_nsec<T, std::void_t<decltype(T::tv_nsec)>> : std::true_type {};

// Static assertions for struct timespec members
static_assert(has_tv_sec<struct timespec>::value,
              "struct timespec must have a 'tv_sec' member");
static_assert(has_tv_nsec<struct timespec>::value,
              "struct timespec must have a 'tv_nsec' member");

// Assert that timespec::tv_sec is of type time_t
static_assert(std::is_same_v<decltype(timespec::tv_sec), time_t>,
              "The type of 'timespec::tv_sec' must be 'time_t'");

// Assert that timespec::tv_nsec is of type long
static_assert(std::is_same_v<decltype(timespec::tv_nsec), long>,
              "The type of 'timespec::tv_nsec' must be 'long'");

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, clock_gettime() is declared "noexcept".
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// Assert that clock_gettime has the signature:
// int clock_gettime(clockid_t, struct timespec*)
static_assert(
    std::is_same_v<decltype(clock_gettime), int(clockid_t, struct timespec*)>,
    "'clock_gettime' is not declared or does not have the signature "
    "'int (clockid_t, struct timespec*)'");
#endif

class AvailableClock
    : public ::testing::TestWithParam<std::tuple<clockid_t, ClockIsRequired>> {
};

TEST_P(AvailableClock, IsSupported) {
  auto param = GetParam();
  clockid_t clock_id = std::get<0>(param);
  ClockIsRequired requirement = std::get<1>(param);
  std::string clock_name = GetClockName(clock_id);

  struct timespec ts{};
  errno = 0;
  int ret = clock_gettime(clock_id, &ts);

  if (ret == -1 && errno == EINVAL &&
      requirement == ClockIsRequired::OPTIONAL) {
    GTEST_SKIP() << "Optional " << clock_name
                 << " is not supported on this system.";
  }
  ASSERT_EQ(0, ret) << "clock_gettime(" << clock_name
                    << ") failed. errno: " << errno << " " << strerror(errno);

  EXPECT_GE(ts.tv_nsec, 0);
  EXPECT_LT(ts.tv_nsec, 1'000'000'000L);
}

INSTANTIATE_TEST_SUITE_P(
    PosixTimeClockGettimeTests,
    AvailableClock,
    ::testing::Values(
        // We also include optional clocks here to verify that they don't crash
        // and return -1 with errno set to EINVAL.
        // Certain wall clocks should be available.
        std::make_tuple(CLOCK_REALTIME, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_REALTIME_ALARM, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_REALTIME_COARSE, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_TAI, ClockIsRequired::OPTIONAL),
        // Certain monotonic clocks also should increase.
        std::make_tuple(CLOCK_MONOTONIC, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_MONOTONIC_RAW, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_MONOTONIC_COARSE, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME_ALARM, ClockIsRequired::OPTIONAL),
        // All CPU clocks should be available.
        std::make_tuple(CLOCK_PROCESS_CPUTIME_ID, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_THREAD_CPUTIME_ID, ClockIsRequired::REQUIRED)),
    [](const ::testing::TestParamInfo<std::tuple<clockid_t, ClockIsRequired>>&
           info) { return GetClockName(std::get<0>(info.param)); });

class IncreasingClock
    : public ::testing::TestWithParam<std::tuple<clockid_t, ClockIsRequired>> {
};

TEST_P(IncreasingClock, Increases) {
  auto param = GetParam();
  clockid_t clock_id = std::get<0>(param);
  ClockIsRequired requirement = std::get<1>(param);
  std::string clock_name = GetClockName(clock_id);

  struct timespec ts_before{};
  errno = 0;
  int ret_before = clock_gettime(clock_id, &ts_before);

  if (ret_before == -1 && errno == EINVAL &&
      requirement == ClockIsRequired::OPTIONAL) {
    GTEST_SKIP() << "Optional " << clock_name
                 << " is not supported on this system.";
  }
  ASSERT_EQ(0, ret_before) << "Initial clock_gettime(" << clock_name
                           << ") failed. errno: " << errno << " "
                           << strerror(errno);
  int64_t time_before_us = TimespecToMicroseconds(ts_before);

  struct timespec sleep_duration = kSleepDuration;
  nanosleep(&sleep_duration, NULL);

  errno = 0;
  struct timespec ts_after{};
  ASSERT_EQ(0, clock_gettime(clock_id, &ts_after))
      << "Second clock_gettime(" << clock_name << ") failed. errno: " << errno
      << " " << strerror(errno);
  int64_t time_after_us = TimespecToMicroseconds(ts_after);

  // Realtime clocks can be adjusted. A significant jump backwards or no
  // increase would be an issue, but small adjustments are possible. The test
  // accepts any measured clock increase, no matter how small. The
  // kSleepDuration is chosen large enough for low resolution and
  // reasonably adjusted clocks to show an increase.
  EXPECT_GT(time_after_us, time_before_us)
      << clock_name << "-based time should at least increase after"
      << kSleepDurationMicroseconds << " us sleep. Before: " << time_before_us
      << " us, After: " << time_after_us << " us.";
}

INSTANTIATE_TEST_SUITE_P(
    PosixTimeClockGettimeTests,
    IncreasingClock,
    ::testing::Values(
        // Wall clocks should increase.
        std::make_tuple(CLOCK_REALTIME, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_REALTIME_ALARM, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_REALTIME_COARSE, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_TAI, ClockIsRequired::OPTIONAL),
        // Monotonic clocks also should increase.
        std::make_tuple(CLOCK_MONOTONIC, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_MONOTONIC_RAW, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_MONOTONIC_COARSE, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME_ALARM, ClockIsRequired::OPTIONAL)),
    [](const ::testing::TestParamInfo<std::tuple<clockid_t, ClockIsRequired>>&
           info) { return GetClockName(std::get<0>(info.param)); });

class MonotonicClock
    : public ::testing::TestWithParam<std::tuple<clockid_t, ClockIsRequired>> {
};

TEST_P(MonotonicClock, IsMonotonic) {
  auto param = GetParam();
  clockid_t clock_id = std::get<0>(param);
  ClockIsRequired requirement = std::get<1>(param);
  std::string clock_name = GetClockName(clock_id);
  const int kTrials = 1'000;

  struct timespec ts_initial{};
  errno = 0;
  int ret_initial = clock_gettime(clock_id, &ts_initial);

  if (ret_initial == -1 && errno == EINVAL &&
      requirement == ClockIsRequired::OPTIONAL) {
    GTEST_SKIP() << "Optional " << clock_name
                 << " is not supported on this system.";
  }
  ASSERT_EQ(0, ret_initial)
      << "Initial clock_gettime(" << clock_name << ") failed. errno: " << errno
      << " " << strerror(errno);
  int64_t initial_time = TimespecToMicroseconds(ts_initial);

  for (int trial = 0; trial < kTrials; ++trial) {
    struct timespec ts_curr{};
    errno = 0;
    ASSERT_EQ(0, clock_gettime(clock_id, &ts_curr))
        << "clock_gettime(" << clock_name << ") in loop failed for trial "
        << trial << ". errno: " << errno << " " << strerror(errno);
    int64_t current_time = TimespecToMicroseconds(ts_curr);

    EXPECT_GE(current_time, initial_time)
        << clock_name
        << "-based time should be non-decreasing. Previous: " << initial_time
        << " us, Current: " << current_time << " us on trial " << trial;

    initial_time = current_time;
  }
}

INSTANTIATE_TEST_SUITE_P(
    PosixTimeClockGettimeTests,
    MonotonicClock,
    ::testing::Values(
        std::make_tuple(CLOCK_MONOTONIC, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_MONOTONIC_RAW, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_MONOTONIC_COARSE, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME, ClockIsRequired::OPTIONAL),
        std::make_tuple(CLOCK_BOOTTIME_ALARM, ClockIsRequired::OPTIONAL),
        // CPU time clocks are also monotonic.
        std::make_tuple(CLOCK_PROCESS_CPUTIME_ID, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_THREAD_CPUTIME_ID, ClockIsRequired::REQUIRED)),
    [](const ::testing::TestParamInfo<std::tuple<clockid_t, ClockIsRequired>>&
           info) { return GetClockName(std::get<0>(info.param)); });

class CpuTimeClock
    : public ::testing::TestWithParam<std::tuple<clockid_t, ClockIsRequired>> {
};

TEST_P(CpuTimeClock, Increases) {
  auto param = GetParam();
  clockid_t clock_id = std::get<0>(param);
  ClockIsRequired requirement = std::get<1>(param);
  std::string clock_name = GetClockName(clock_id);

  errno = 0;
  struct timespec ts_before{};
  int initial_ret = clock_gettime(clock_id, &ts_before);
  if (initial_ret == -1 && errno == EINVAL &&
      requirement == ClockIsRequired::OPTIONAL) {
    GTEST_SKIP() << "Optional " << clock_name
                 << " is not supported on this system.";
  }
  ASSERT_EQ(0, initial_ret)
      << "Initial clock_gettime(" << clock_name << ") failed. errno: " << errno
      << " " << strerror(errno);

  long elapsed_time_us = ConsumeCpuForDuration(kMinimumWorkTimeMicroseconds);
  EXPECT_GE(elapsed_time_us, kMinimumWorkTimeMicroseconds)
      << "Elapsed time shorter than the minimum "
      << kMinimumWorkTimeMicroseconds << " us";

  errno = 0;
  struct timespec ts_after{};
  ASSERT_EQ(0, clock_gettime(clock_id, &ts_after))
      << "Final clock_gettime(" << clock_name << ") failed. errno: " << errno
      << " " << strerror(errno);

  int64_t time_before_us = TimespecToMicroseconds(ts_before);
  int64_t time_after_us = TimespecToMicroseconds(ts_after);

  EXPECT_GT(time_after_us, time_before_us)
      << clock_name << " time should have increased.";
  // Expect to have measured at least 75% of the elapsed time as CPU time.
  // Note: This will fail on a busy system where this test receives less than
  // 75% of a CPU core.
  long measured_work_time = time_after_us - time_before_us;
  long minimum_work_time = 0.75 * elapsed_time_us;
  EXPECT_GE(measured_work_time, minimum_work_time)
      << clock_name << " time should measure at least  " << minimum_work_time
      << " us, of " << elapsed_time_us << " us elapsed during CPU work.";
}

INSTANTIATE_TEST_SUITE_P(
    PosixTimeClockGettimeTests,
    CpuTimeClock,
    ::testing::Values(
        std::make_tuple(CLOCK_PROCESS_CPUTIME_ID, ClockIsRequired::REQUIRED),
        std::make_tuple(CLOCK_THREAD_CPUTIME_ID, ClockIsRequired::REQUIRED)),
    [](const ::testing::TestParamInfo<std::tuple<clockid_t, ClockIsRequired>>&
           info) { return GetClockName(std::get<0>(info.param)); });

TEST(PosixTimeClockGettimeTests, ReturnsEinvalForInvalidClockId) {
  struct timespec ts{};

  // A large positive integer unlikely to be a valid clock ID.
  const clockid_t kInvalidPositiveClockId = 0xDEFEC8ED;
  errno = 0;
  EXPECT_EQ(-1, clock_gettime(kInvalidPositiveClockId, &ts));
  EXPECT_EQ(EINVAL, errno);

  // A negative integer unlikely to be a valid clock ID.
  const clockid_t kInvalidNegativeClockId = -4242;
  errno = 0;
  EXPECT_EQ(-1, clock_gettime(kInvalidNegativeClockId, &ts));
  EXPECT_EQ(EINVAL, errno);
}

// Comment on EPERM:
// The EPERM error is generally not applicable to clock_gettime() for reading
// standard, publicly accessible clocks (like CLOCK_REALTIME, CLOCK_MONOTONIC)
// or the calling process/thread's CPU time (CLOCK_PROCESS_CPUTIME_ID,
// CLOCK_THREAD_CPUTIME_ID). EPERM might arise if trying to get a CPU-time
// clock ID for *another* process (obtained via clock_getcpuclockid()) for which
// the caller lacks permissions. Testing that scenario is more complex as it
// involves inter-process permissions and multiple function calls, and is
// outside the direct scope of testing clock_gettime with its predefined clock
// IDs. Therefore, a specific EPERM test for clock_gettime with these standard
// IDs is not included.

}  // namespace
}  // namespace nplb
