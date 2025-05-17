// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
#include <time.h>
#include <unistd.h>  // For syscall

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Small duration for sleeps that should be short or return immediately.
const long kShortSleepNs = 1000000;  // 1 millisecond
// A slightly longer duration for testing actual sleep.
const long kTestSleepNs = 50000000;  // 50 milliseconds
// A very long duration for testing interruption.
const long kLongSleepSec = 10;  // 10 seconds

// Note: The return value EINTR is not tested here because Starboard does not
// implement signal handling.

// Test successful relative sleep with CLOCK_MONOTONIC
TEST(PosixClockNanosleepTest, RelativeSleepMonotonicClock) {
  struct timespec req = {0, kTestSleepNs};  // 50 milliseconds
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);
}

// Test successful relative sleep with CLOCK_REALTIME
TEST(PosixClockNanosleepTest, RelativeSleepRealtimeClock) {
  struct timespec req = {0, kTestSleepNs};  // 50 milliseconds
  struct timespec rem;
  clockid_t clock_id = CLOCK_REALTIME;
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);
}

// Test successful absolute sleep with CLOCK_MONOTONIC
TEST(PosixClockNanosleepTest, AbsoluteSleepMonotonicClock) {
  struct timespec req;
  struct timespec rem;  // Should not be used for absolute sleep
  clockid_t clock_id = CLOCK_MONOTONIC;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  // Set an absolute time 50ms in the future
  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }

  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);
}

// Test successful absolute sleep with CLOCK_REALTIME
TEST(PosixClockNanosleepTest, AbsoluteSleepRealtimeClock) {
  struct timespec req;
  struct timespec rem;  // Should not be used for absolute sleep
  clockid_t clock_id = CLOCK_REALTIME;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_REALTIME";

  // Set an absolute time 50ms in the future
  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }

  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);
}

// Test immediate return for absolute sleep when requested time is in the past
TEST(PosixClockNanosleepTest, AbsoluteSleepTimeInPastReturnsImmediately) {
  struct timespec req;
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  // Set an absolute time in the past (or very near present)
  if (req.tv_nsec >= kShortSleepNs) {
    req.tv_nsec -= kShortSleepNs;
  } else if (req.tv_sec > 0) {
    req.tv_sec--;
    req.tv_nsec += (1000000000 - kShortSleepNs);
  }
  // else req is very close to 0,0 so it's effectively in the past or immediate

  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected immediate return for past absolute time, got: "
                    << strerror(ret);
}

// Test EFAULT for request argument
TEST(PosixClockNanosleepTest, ErrorEfaultForRequest) {
  struct timespec* bad_req =
      reinterpret_cast<struct timespec*>(0x1);  // Invalid address
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, bad_req, &rem);
  // EFAULT might not be reliably triggered in all sandboxed environments
  // or could cause a crash. Gtest death tests could be an alternative
  // but are more complex. Here we expect it to fail and set errno.
  // POSIX specifies EFAULT, but behavior might vary slightly.
  if (ret != 0) {  // If it didn't succeed (which it shouldn't)
    EXPECT_EQ(EFAULT, ret) << "Expected EFAULT for bad request pointer, got: "
                           << strerror(ret);
  } else {
    // This case is highly unlikely for a truly invalid pointer like 0x1,
    // but included for completeness. It would indicate the OS didn't fault.
    FAIL() << "clock_nanosleep succeeded with a known bad request pointer.";
  }
}

// Test EINVAL for invalid nanoseconds in request (negative)
TEST(PosixClockNanosleepTest, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1};  // Invalid nanoseconds
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for negative ns, got: "
                         << strerror(ret);
}

// Test EINVAL for invalid nanoseconds in request (too large)
TEST(PosixClockNanosleepTest, ErrorEinvalRequestNsTooLarge) {
  struct timespec req = {0, 1000000000};  // Invalid nanoseconds (>= 10^9)
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for ns >= 10^9, got: "
                         << strerror(ret);
}

// Test EINVAL for negative seconds in request with TIMER_ABSTIME
// This specific condition (tv_sec < 0 for TIMER_ABSTIME) is not explicitly
// listed as EINVAL in POSIX for clock_nanosleep, but it's implied by "rqtp
// argument is outside the range for the clock". However, a negative absolute
// time might be valid for some clocks if their epoch allows it, though
// practically it would be in the past and return immediately. More reliably,
// tv_nsec out of range is EINVAL. Linux man page for clock_nanosleep(2) states
// for EINVAL: "The value in the tv_nsec field was not in the range 0 to
// 999,999,999
//  or tv_sec was negative." This applies to the 'request' timespec.
TEST(PosixClockNanosleepTest, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0};  // Invalid seconds
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  // For relative sleep, a negative tv_sec is an invalid interval.
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret)
      << "Expected EINVAL for negative tv_sec in relative sleep, got: "
      << strerror(ret);
}

// Test EINVAL for invalid clock_id
TEST(PosixClockNanosleepTest, ErrorEinvalInvalidClockId) {
  struct timespec req = {0, kShortSleepNs};  // 1 millisecond
  struct timespec rem;
  clockid_t bad_clock_id = -123;  // An unlikely to be valid clock ID
  int ret = clock_nanosleep(bad_clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid clock_id, got: "
                         << strerror(ret);
}

// Test EINVAL for invalid flags
TEST(PosixClockNanosleepTest, ErrorEinvalInvalidFlags) {
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int invalid_flags = 0xFF;  // Some likely invalid flags
  // POSIX only defines TIMER_ABSTIME. Other bits should be 0.
  // Some systems might tolerate other bits if TIMER_ABSTIME is correctly
  // set/unset. The most reliable way to get EINVAL for flags is usually an
  // undefined flag value. If flags has bits set other than TIMER_ABSTIME, it's
  // technically invalid.
  int ret = clock_nanosleep(clock_id, invalid_flags, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid flags, got: "
                         << strerror(ret);
}

// Test ENOTSUP for a clock that might not be supported for nanosleep,
// like a CPU-time clock.
TEST(PosixClockNanosleepTest, ErrorEnotsupForCpuClock) {
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  clockid_t cpu_clock_id;

  // Attempt to get a CPU clock ID.
  // clock_getcpuclockid(0, &cpu_clock_id) gets for a process,
  // pthread_getcpuclockid(pthread_self(), &cpu_clock_id) for a thread.
  // Using the calling process's CPU-time clock ID (if available and distinct)
  // or a known thread CPU-time clock ID.
  // POSIX states: "The clock_nanosleep() function shall fail if the clock_id
  // argument refers to the CPU-time clock of the calling thread."
  // Also: "It is unspecified whether clock_id values of other CPU-time clocks
  // are allowed." And: "[ENOTSUP] The clock_id argument specifies a clock for
  // which clock_nanosleep() is not supported, such as a CPU-time clock."

  // Try with CLOCK_PROCESS_CPUTIME_ID first as it's more general.
  // If CLOCK_PROCESS_CPUTIME_ID itself is not supported for clock_nanosleep
  // or if it specifically refers to the calling process's CPU time, it should
  // give ENOTSUP.
  cpu_clock_id = CLOCK_PROCESS_CPUTIME_ID;
  int ret = clock_nanosleep(cpu_clock_id, 0, &req, &rem);

  // Some systems might return EINVAL if CLOCK_PROCESS_CPUTIME_ID is not a
  // "known clock" in this context, or ENOTSUP if it's known but not usable for
  // sleep. The man page is more specific about ENOTSUP for CPU-time clocks.
  EXPECT_TRUE(ret == ENOTSUP || ret == EINVAL)
      << "Expected ENOTSUP or EINVAL for CLOCK_PROCESS_CPUTIME_ID, got: "
      << strerror(ret);
}

// Test behavior with NULL remain and relative sleep (should be fine)
TEST(PosixClockNanosleepTest, RelativeSleepNullRemain) {
  struct timespec req = {0, kShortSleepNs};  // 1 millisecond
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, &req, nullptr);
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain, got: "
                    << strerror(ret);
}

// Test behavior with NULL remain and absolute sleep (should be fine)
TEST(PosixClockNanosleepTest, AbsoluteSleepNullRemain) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;
  ASSERT_EQ(0, clock_gettime(clock_id, &req));
  req.tv_nsec += kShortSleepNs;  // Sleep for 1ms in the future
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, nullptr);
  EXPECT_EQ(0, ret)
      << "Expected successful absolute sleep with NULL remain, got: "
      << strerror(ret);
}

// Test relative sleep with zero duration (should return immediately)
TEST(PosixClockNanosleepTest, RelativeSleepZeroDuration) {
  struct timespec req = {0, 0};  // Zero duration
  struct timespec rem;
  clockid_t clock_id = CLOCK_MONOTONIC;
  int ret = clock_nanosleep(clock_id, 0, &req, &rem);
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration relative sleep, got: "
      << strerror(ret);
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
