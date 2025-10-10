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
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixPrctlNameTests : public ::testing::Test {
 protected:
  void SetUp() override {
    memset(original_name_, 0, sizeof(original_name_));
    errno = 0;
    int result = prctl(PR_GET_NAME, original_name_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_GET_NAME) failed in SetUp. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    // Restore the original thread name to avoid side effects.
    int result = prctl(PR_SET_NAME, original_name_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_NAME) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  char original_name_[16];
};

TEST_F(PosixPrctlNameTests, SetAndGetSuccessRoundTrip) {
  const char* kNewName = "MyTestThread";
  char buffer[16];
  memset(buffer, 0, sizeof(buffer));

  errno = 0;
  int result = prctl(PR_SET_NAME, kNewName);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_NAME) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  result = prctl(PR_GET_NAME, buffer);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_GET_NAME) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  EXPECT_STREQ(kNewName, buffer);
}

TEST_F(PosixPrctlNameTests, SetNameTruncatesLongName) {
  // This name is 17 characters, which is 18 bytes with the null terminator.
  // It should be truncated to 15 characters (16 bytes with null).
  const char* kLongName = "12345678901234567";
  const char* kExpectedName = "123456789012345";  // 15 chars
  char buffer[16];
  memset(buffer, 0, sizeof(buffer));

  errno = 0;
  int result = prctl(PR_SET_NAME, kLongName);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_NAME) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  result = prctl(PR_GET_NAME, buffer);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_GET_NAME) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  EXPECT_STREQ(kExpectedName, buffer);
}

TEST_F(PosixPrctlNameTests, SetNameFailsWithNull) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_NAME, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for NULL name, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixPrctlNameTests, GetNameFailsWithNull) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_GET_NAME, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for NULL buffer, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixPrctlNameTests, FailsWithInvalidOption) {
  errno = 0;
  EXPECT_EQ(-1, prctl(-1, 0, 0, 0, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid option, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

class PosixPrctlPdeathsigTests : public ::testing::Test {
 protected:
  void SetUp() override {
    original_pdeathsig_ = 0;
    errno = 0;
    int result = prctl(PR_GET_PDEATHSIG, &original_pdeathsig_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_GET_PDEATHSIG) failed in SetUp. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    // Restore the original parent death signal to avoid side effects.
    int result = prctl(PR_SET_PDEATHSIG, original_pdeathsig_);
    int call_errno = errno;
    ASSERT_EQ(0, result)
        << "prctl(PR_SET_PDEATHSIG) failed in TearDown. Errno: " << call_errno
        << " (" << strerror(call_errno) << ")";
  }

  int original_pdeathsig_;
};

TEST_F(PosixPrctlPdeathsigTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  int result = prctl(PR_SET_PDEATHSIG, SIGHUP);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_PDEATHSIG) failed. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";

  int current_sig = 0;
  errno = 0;
  result = prctl(PR_GET_PDEATHSIG, &current_sig);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_GET_PDEATHSIG) failed. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";

  EXPECT_EQ(SIGHUP, current_sig);
}

TEST_F(PosixPrctlPdeathsigTests, SetFailsWithInvalidSignal) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_PDEATHSIG, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid signal, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixPrctlPdeathsigTests, GetFailsWithNullArgument) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_GET_PDEATHSIG, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for NULL argument, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the dumpable flag.
class PosixPrctlDumpableTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_dumpable_ = prctl(PR_GET_DUMPABLE);
    int call_errno = errno;
    ASSERT_NE(-1, original_dumpable_)
        << "prctl(PR_GET_DUMPABLE) failed in SetUp. Errno: " << call_errno
        << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    // Restore the original dumpable flag to avoid side effects.
    int result = prctl(PR_SET_DUMPABLE, original_dumpable_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_DUMPABLE) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  int original_dumpable_;
};

TEST_F(PosixPrctlDumpableTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  int current_dumpable = prctl(PR_GET_DUMPABLE);
  int call_errno = errno;
  ASSERT_NE(-1, current_dumpable)
      << "prctl(PR_GET_DUMPABLE) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  // Toggle the value between 0 and 1.
  int new_value = (current_dumpable == 0) ? 1 : 0;

  errno = 0;
  int result = prctl(PR_SET_DUMPABLE, new_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_DUMPABLE) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  int retrieved_value = prctl(PR_GET_DUMPABLE);
  call_errno = errno;
  ASSERT_NE(-1, retrieved_value)
      << "prctl(PR_GET_DUMPABLE) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, retrieved_value);
}

TEST_F(PosixPrctlDumpableTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_DUMPABLE, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_DUMPABLE, 2));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the keep-capabilities flag.
class PosixPrctlKeepcapsTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_keepcaps_ = prctl(PR_GET_KEEPCAPS);
    int call_errno = errno;
    ASSERT_NE(-1, original_keepcaps_)
        << "prctl(PR_GET_KEEPCAPS) failed in SetUp. Errno: " << call_errno
        << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    // Restore the original keepcaps flag to avoid side effects.
    int result = prctl(PR_SET_KEEPCAPS, original_keepcaps_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_KEEPCAPS) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  long original_keepcaps_;
};

TEST_F(PosixPrctlKeepcapsTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  long current_keepcaps = prctl(PR_GET_KEEPCAPS);
  int call_errno = errno;
  ASSERT_NE(-1, current_keepcaps)
      << "prctl(PR_GET_KEEPCAPS) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  // Toggle the value between 0 and 1.
  long new_value = (current_keepcaps == 0) ? 1 : 0;

  errno = 0;
  int result = prctl(PR_SET_KEEPCAPS, new_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_KEEPCAPS) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  long retrieved_value = prctl(PR_GET_KEEPCAPS);
  call_errno = errno;
  ASSERT_NE(-1, retrieved_value)
      << "prctl(PR_GET_KEEPCAPS) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, retrieved_value);
}

TEST_F(PosixPrctlKeepcapsTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_KEEPCAPS, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_KEEPCAPS, 2));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the floating-point emulation control flag.
class PosixPrctlFpemuTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_fpemu_ = prctl(PR_GET_FPEMU);
    int call_errno = errno;
    // This call can fail with EINVAL if the kernel doesn't support it.
    if (original_fpemu_ == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_GET_FPEMU) is not supported.";
    }
    ASSERT_NE(-1, original_fpemu_)
        << "prctl(PR_GET_FPEMU) failed in SetUp. Errno: " << call_errno << " ("
        << strerror(call_errno) << ")";
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    errno = 0;
    // Restore the original fpemu flag to avoid side effects.
    int result = prctl(PR_SET_FPEMU, original_fpemu_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_FPEMU) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  int original_fpemu_;
};

TEST_F(PosixPrctlFpemuTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  int current_fpemu = prctl(PR_GET_FPEMU);
  int call_errno = errno;
  ASSERT_NE(-1, current_fpemu)
      << "prctl(PR_GET_FPEMU) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  // Toggle the value between the two valid states.
  int new_value =
      (current_fpemu == PR_FPEMU_NOPRINT) ? PR_FPEMU_SIGFPE : PR_FPEMU_NOPRINT;

  errno = 0;
  int result = prctl(PR_SET_FPEMU, new_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_FPEMU) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  int retrieved_value = prctl(PR_GET_FPEMU);
  call_errno = errno;
  ASSERT_NE(-1, retrieved_value)
      << "prctl(PR_GET_FPEMU) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, retrieved_value);
}

TEST_F(PosixPrctlFpemuTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_FPEMU, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_FPEMU, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_FPEMU, 3));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}
TEST_F(PosixPrctlFpemuTests, GetFailsWithInvalidAddress) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_GET_FPEMU, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for invalid address, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the process timing method.
class PosixPrctlTimingTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_timing_ = prctl(PR_GET_TIMING);
    int call_errno = errno;
    // This call can fail with EINVAL if the kernel doesn't support it.
    if (original_timing_ == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_GET_TIMING) is not supported.";
    }
    ASSERT_NE(-1, original_timing_)
        << "prctl(PR_GET_TIMING) failed in SetUp. Errno: " << call_errno << " ("
        << strerror(call_errno) << ")";
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    errno = 0;
    // Restore the original timing flag to avoid side effects.
    int result = prctl(PR_SET_TIMING, original_timing_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_TIMING) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  int original_timing_;
};

TEST_F(PosixPrctlTimingTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  int current_timing = prctl(PR_GET_TIMING);
  int call_errno = errno;
  ASSERT_NE(-1, current_timing)
      << "prctl(PR_GET_TIMING) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  // Toggle the value between the two valid states.
  long new_value = PR_TIMING_STATISTICAL;

  errno = 0;
  int result = prctl(PR_SET_TIMING, new_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_TIMING) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  errno = 0;
  int retrieved_value = prctl(PR_GET_TIMING);
  call_errno = errno;
  ASSERT_NE(-1, retrieved_value)
      << "prctl(PR_GET_TIMING) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, retrieved_value);
}

TEST_F(PosixPrctlTimingTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_TIMING, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_TIMING, 2));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// According to the man-pages, |PR_TIMING_TIMESTAMP| is not implemented. This
// test ensures that this behavior is preserved.
TEST_F(PosixPrctlTimingTests, SetFailsWithTimingTimestamp) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_TIMING, PR_TIMING_TIMESTAMP));
  EXPECT_EQ(EINVAL, errno)
      << "Expected EINVAL for PR_TIMING_TIMESTAMP, but got: " << errno << " ("
      << strerror(errno) << ")";
}

// Test fixture to save and restore the timestamp counter control flag.
class PosixPrctlTscTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    int arg2 = 0;
    int ret = prctl(PR_GET_TSC, &arg2);
    original_tsc_ = arg2;
    int call_errno = errno;
    // This call can fail with EINVAL if the kernel doesn't support it.
    if (ret == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_GET_TSC) is not supported.";
    }
    ASSERT_NE(-1, ret) << "prctl(PR_GET_TSC) failed in SetUp. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    errno = 0;
    // Restore the original tsc flag to avoid side effects.
    int result = prctl(PR_SET_TSC, original_tsc_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_TSC) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  int original_tsc_;
};

TEST_F(PosixPrctlTscTests, SetAndGetSuccessRoundTrip) {
  // This test only makes use of the value PR_TSC_ENABLE. PR_TSC_SIGSEGV is not
  // tested as if we try to call PR_GET_TSC with PR_TSC_SIGSEGV, the suite
  // crashes with a SIGSEGV error.
  int new_value = PR_TSC_ENABLE;

  errno = 0;
  int result = prctl(PR_SET_TSC, new_value);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_TSC) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  int current_tsc = 0;
  errno = 0;
  int ret = prctl(PR_GET_TSC, &current_tsc);
  call_errno = errno;
  ASSERT_NE(-1, ret) << "prctl(PR_GET_TSC) failed. Errno: " << call_errno
                     << " (" << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, current_tsc);
}

TEST_F(PosixPrctlTscTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_TSC, -1000));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixPrctlTscTests, GetFailsWithNullArgument) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_GET_TSC, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for NULL argument, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the per-thread timer slack value.
class PosixPrctlTimerslackTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_timerslack_ = prctl(PR_GET_TIMERSLACK);
    int call_errno = errno;
    ASSERT_NE(-1, original_timerslack_)
        << "prctl(PR_GET_TIMERSLACK) failed in SetUp. Errno: " << call_errno
        << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    // Restore the original timerslack value to avoid side effects.
    int result = prctl(PR_SET_TIMERSLACK, original_timerslack_);
    int call_errno = errno;
    ASSERT_EQ(0, result)
        << "prctl(PR_SET_TIMERSLACK) failed in TearDown. Errno: " << call_errno
        << " (" << strerror(call_errno) << ")";
  }

  int original_timerslack_;
};

TEST_F(PosixPrctlTimerslackTests, SetAndGetSuccessRoundTrip) {
  // A new valid slack value in nanoseconds.
  unsigned long new_value = 100000;
  if (static_cast<unsigned long>(original_timerslack_) == new_value) {
    new_value = 50000;  // Default value, likely different.
  }

  errno = 0;
  int result = prctl(PR_SET_TIMERSLACK, new_value);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_TIMERSLACK) failed. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";

  errno = 0;
  int retrieved_value = prctl(PR_GET_TIMERSLACK);
  call_errno = errno;
  ASSERT_NE(-1, retrieved_value)
      << "prctl(PR_GET_TIMERSLACK) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, static_cast<unsigned long>(retrieved_value));
}

// Test fixture for task performance events control.
class PosixPrctlTaskPerfEventsTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    // Try one of the operations to see if it's supported.
    int result = prctl(PR_TASK_PERF_EVENTS_DISABLE);
    int call_errno = errno;
    if (result == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl task performance events are not supported.";
    }
    // If it worked, immediately re-enable to restore state before the test.
    if (result == 0) {
      prctl(PR_TASK_PERF_EVENTS_ENABLE);
    }
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    // Leave the state as enabled, which is a sane default.
    prctl(PR_TASK_PERF_EVENTS_ENABLE);
  }
};

TEST_F(PosixPrctlTaskPerfEventsTests, Success) {
  errno = 0;
  int result = prctl(PR_TASK_PERF_EVENTS_DISABLE);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_TASK_PERF_EVENTS_DISABLE) failed. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";

  errno = 0;
  result = prctl(PR_TASK_PERF_EVENTS_ENABLE);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_TASK_PERF_EVENTS_ENABLE) failed. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";
}

// Test fixture for PR_SET_PTRACER.
class PosixPrctlPtracerTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    // Check if the operation is supported by attempting to disable it.
    int result = prctl(PR_SET_PTRACER, 0);
    int call_errno = errno;
    if (result == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_SET_PTRACER) is not supported.";
    }
    ASSERT_EQ(0, result) << "prctl(PR_SET_PTRACER, 0) failed in SetUp. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    // Disable any ptracer that might have been set during a test.
    prctl(PR_SET_PTRACER, 0);
  }
};

TEST_F(PosixPrctlPtracerTests, SetSuccess) {
  errno = 0;
  int result = prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) failed. "
                          "Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";
}

TEST_F(PosixPrctlPtracerTests, SetFailsWithInvalidValue) {
  errno = 0;
  // A PID of -1 is not valid for this operation.
  EXPECT_EQ(-1, prctl(PR_SET_PTRACER, -100000));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Test fixture to save and restore the endian mode.
class PosixPrctlEndianTests : public ::testing::Test {
 protected:
  void SetUp() override {
    original_endian_ = 0;
    errno = 0;
    int result = prctl(PR_GET_ENDIAN, &original_endian_);
    int call_errno = errno;
    if (result == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_GET_ENDIAN) is not supported.";
    }
    ASSERT_EQ(0, result) << "prctl(PR_GET_ENDIAN) failed in SetUp. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    errno = 0;
    // Restore the original endian mode to avoid side effects.
    int result = prctl(PR_SET_ENDIAN, original_endian_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_ENDIAN) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  int original_endian_;
};

TEST_F(PosixPrctlEndianTests, SetAndGetSuccessRoundTrip) {
  int current_endian = 0;
  errno = 0;
  int result = prctl(PR_GET_ENDIAN, &current_endian);
  int call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_GET_ENDIAN) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  // Toggle the value between big and little endian.
  int new_value =
      (current_endian == PR_ENDIAN_BIG) ? PR_ENDIAN_LITTLE : PR_ENDIAN_BIG;

  errno = 0;
  result = prctl(PR_SET_ENDIAN, new_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_SET_ENDIAN) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  int retrieved_value = 0;
  errno = 0;
  result = prctl(PR_GET_ENDIAN, &retrieved_value);
  call_errno = errno;
  ASSERT_EQ(0, result) << "prctl(PR_GET_ENDIAN) failed. Errno: " << call_errno
                       << " (" << strerror(call_errno) << ")";

  EXPECT_EQ(new_value, retrieved_value);
}

TEST_F(PosixPrctlEndianTests, SetFailsWithInvalidValue) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_ENDIAN, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_ENDIAN, -1));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_ENDIAN, 3));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixPrctlEndianTests, GetFailsWithNullArgument) {
  errno = 0;
  EXPECT_EQ(-1, prctl(PR_GET_ENDIAN, nullptr));
  EXPECT_EQ(EFAULT, errno) << "Expected EFAULT for NULL argument, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
