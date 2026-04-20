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

TEST(PosixPrctlGeneralTests, FailsWithInvalidOption) {
  errno = 0;
  EXPECT_EQ(-1, prctl(-1, 0, 0, 0, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid option, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

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
  const char* kLongName = "12345678901234567";
  const char* kExpectedName = "123456789012345";
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
    int result = prctl(PR_SET_DUMPABLE, original_dumpable_);
    int call_errno = errno;
    ASSERT_EQ(0, result) << "prctl(PR_SET_DUMPABLE) failed in TearDown. Errno: "
                         << call_errno << " (" << strerror(call_errno) << ")";
  }

  long original_dumpable_;
};

TEST_F(PosixPrctlDumpableTests, SetAndGetSuccessRoundTrip) {
  errno = 0;
  int current_dumpable = prctl(PR_GET_DUMPABLE);
  int call_errno = errno;
  ASSERT_NE(-1, current_dumpable)
      << "prctl(PR_GET_DUMPABLE) failed. Errno: " << call_errno << " ("
      << strerror(call_errno) << ")";

  long new_value = (current_dumpable == 0L) ? 1L : 0L;

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

class PosixPrctlTimingTests : public ::testing::Test {
 protected:
  void SetUp() override {
    errno = 0;
    original_timing_ = prctl(PR_GET_TIMING);
    int call_errno = errno;
    ASSERT_NE(-1, original_timing_)
        << "prctl(PR_GET_TIMING) failed in SetUp. Errno: " << call_errno << " ("
        << strerror(call_errno) << ")";
  }

  void TearDown() override {
    errno = 0;
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

  // Since the man-pages specify that that |PR_TIMING_TIMESTAMP| is not
  // implemented, we can only test whether or not we can successfully set
  // |PR_TIMING_STATISTICAL|.
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
  EXPECT_EQ(-1, prctl(PR_SET_TIMING, -1000));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";

  errno = 0;
  EXPECT_EQ(-1, prctl(PR_SET_TIMING, 2000));
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

// The man-pages specify that these operations only exist on x86 platforms.
#if defined(PR_GET_TSC) && defined(PR_SET_TSC)
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
#endif  // defined(PR_GET_TSC) && defined(PR_SET_TSC)

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
    new_value = 50000;
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

class PosixPrctlTaskPerfEventsTests : public ::testing::Test {
 protected:
  void TearDown() override {
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

class PosixPrctlPtracerTests : public ::testing::Test {
 protected:
  void SetUp() override {
    int ret = prctl(PR_SET_PTRACER, 0);
    int call_errno = errno;

    if (ret == -1 && call_errno == EINVAL) {
      GTEST_SKIP() << "prctl(PR_SET_PTRACER) is not supported.";
    }
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
  EXPECT_EQ(-1, prctl(PR_SET_PTRACER, -100000));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid value, but got: "
                           << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
