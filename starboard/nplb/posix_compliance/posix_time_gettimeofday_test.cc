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

#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// kReasonableMinTime represents a time (2025-01-01 00:00:00 UTC) after which
// the current time is expected to fall.
const time_t kReasonableMinTime = 1'735'689'600;  // 2025-01-01 00:00:00 UTC

// Helper template to check for the existence of tv_sec
template <typename T, typename = void>
struct has_tv_sec : std::false_type {};
template <typename T>
struct has_tv_sec<T, std::void_t<decltype(T::tv_sec)>> : std::true_type {};

// Helper template to check for the existence of tv_usec
template <typename T, typename = void>
struct has_tv_usec : std::false_type {};
template <typename T>
struct has_tv_usec<T, std::void_t<decltype(T::tv_usec)>> : std::true_type {};

// Assert that the required struct timeval members exist.
static_assert(has_tv_sec<struct timeval>::value,
              "struct timeval must have a 'tv_sec' member");

static_assert(has_tv_usec<struct timeval>::value,
              "struct timeval must have a 'tv_usec' member");

// Assert that the struct timeval members have the correct types.
static_assert(std::is_same_v<decltype(timeval::tv_sec), time_t>,
              "The type of 'timeval::tv_sec' must be 'time_t'");

static_assert(std::is_same_v<decltype(timeval::tv_usec), suseconds_t>,
              "The type of 'timeval::tv_usec' must be 'suseconds_t'");

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, clock_gettime() is declared "noexcept".
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// 5. Assert that gettimeofday has the signature:
// int gettimeofday(struct timeval*, void*)
static_assert(
    std::is_same_v<decltype(gettimeofday), int(struct timeval*, void*)>,
    "'gettimeofday' is not declared or does not have the signature "
    "'int (struct timeval*, struct timezone*)'");
#endif

TEST(PosixTimeGettimeofdayTests, SunnyDay) {
  struct timeval tv{};
  errno = 0;
  ASSERT_EQ(0, gettimeofday(&tv, nullptr))
      << "gettimeofday() failed. errno: " << errno << " " << strerror(errno);

  EXPECT_GT(tv.tv_sec, kReasonableMinTime)
      << "Current time (" << tv.tv_sec
      << ") is unexpectedly earlier than kReasonableMinTime ("
      << kReasonableMinTime << ").";
  EXPECT_GE(tv.tv_usec, 0) << "tv_usec should be non-negative.";
  EXPECT_LE(tv.tv_usec, 999999) << "tv_usec should be less than 1,000,000.";
}

TEST(PosixTimeGettimeofdayTests, NonNullTzIsIgnoredOrCausesEinval) {
  struct timeval tv{};
  struct timezone tz{};

  errno = 0;
  int result = gettimeofday(&tv, &tz);

  if (result == -1) {
    EXPECT_EQ(EINVAL, errno)
        << "Expected EINVAL when tz is non-nullptr and not supported, "
        << "but got errno: " << errno << " " << strerror(errno);
  } else {
    EXPECT_EQ(0, result)
        << "gettimeofday with non-nullptr tz failed with an unexpected result "
        << result << " and errno: " << errno << " " << strerror(errno);
    EXPECT_GE(tv.tv_sec, kReasonableMinTime);
    EXPECT_GE(tv.tv_usec, 0);
    EXPECT_LE(tv.tv_usec, 999999);
  }
}

// Note on EFAULT for tz:
// EFAULT could occur if tz is a non-nullptr, invalid pointer, AND the system
// attempts to dereference it. However, the tz parameter is obsolete.
// POSIX.1-2008 specifies that gettimeofday() should fail with EINVAL if tz
// is non-nullptr and timezones are not supported through this interface.
// Most implementations will likely check if tz is non-nullptr and either return
// EINVAL or ignore tz without dereferencing it. Therefore, reliably
// triggering EFAULT due to an invalid tz pointer (while tv is valid) is
// difficult to test portably and is not a primary compliance concern since
// tz should always be nullptr in modern usage.

// Tests the resolution of time obtained via gettimeofday().
TEST(PosixTimeGettimeofdayTests, GettimeofdayHasDecentResolution) {
  // Number of iterations to check for time change.
  // Chosen to be large enough to observe time changes on typical systems
  // but small enough to not make the test excessively long.
  const int kNumIterations = 100;
  for (int i = 0; i < kNumIterations; ++i) {
    struct timespec monotonic_ts_start{};
    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &monotonic_ts_start))
        << "clock_gettime(CLOCK_MONOTONIC) for timerStart failed. errno: "
        << errno;
    int64_t timerStart =
        (static_cast<int64_t>(monotonic_ts_start.tv_sec) * 1'000'000LL) +
        (static_cast<int64_t>(monotonic_ts_start.tv_nsec) / 1'000LL);

    struct timeval posix_tv_initial{};
    ASSERT_EQ(0, gettimeofday(&posix_tv_initial, nullptr))
        << "gettimeofday() for initialTime failed. errno: " << errno << " "
        << strerror(errno);
    int64_t initialTime =
        (static_cast<int64_t>(posix_tv_initial.tv_sec) * 1000000LL) +
        posix_tv_initial.tv_usec;

    int64_t currentTime = initialTime;

    const int64_t kMaxResolutionMicroseconds = 100'000LL;  // 100ms
    // Loop until the time reported by gettimeofday changes.
    while (currentTime == initialTime) {
      struct timeval posix_tv_current{};
      ASSERT_EQ(0, gettimeofday(&posix_tv_current, nullptr))
          << "gettimeofday() in loop failed. errno: " << errno << " "
          << strerror(errno);
      currentTime =
          (static_cast<int64_t>(posix_tv_current.tv_sec) * 1000000LL) +
          posix_tv_current.tv_usec;

      struct timespec monotonic_ts_current{};
      ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &monotonic_ts_current))
          << "clock_gettime(CLOCK_MONOTONIC) in loop failed. errno: " << errno
          << " " << strerror(errno);
      int64_t monotonicTimeNow =
          (static_cast<int64_t>(monotonic_ts_current.tv_sec) * 1'000'000LL) +
          (static_cast<int64_t>(monotonic_ts_current.tv_nsec) / 1'000LL);

      int64_t monotonic_time_elapsed_us = monotonicTimeNow - timerStart;
      ASSERT_LT(monotonic_time_elapsed_us, kMaxResolutionMicroseconds)
          << "gettimeofday()-based time hasn't changed within "
          << (kMaxResolutionMicroseconds / 1'000L) << " ms "
          << "during iteration " << i << ". Initial time: " << initialTime
          << ", Elapsed monotonic time: " << monotonic_time_elapsed_us / 1'000L
          << " ms.";
    }
  }
}

}  // namespace
}  // namespace nplb
