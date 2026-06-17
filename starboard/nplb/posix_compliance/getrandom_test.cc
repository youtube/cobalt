// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include <sys/random.h>

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(GetRandomTest, FillsBuffer) {
  const ssize_t kBufSize = 256;
  std::vector<char> buf1(kBufSize, 0);
  std::vector<char> buf2(kBufSize, 0);

  ssize_t result1 = getrandom(buf1.data(), buf1.size(), 0);
  if (result1 == -1 && errno == ENOSYS) {
    GTEST_SKIP() << "getrandom() not supported on this platform.";
  }
  ASSERT_EQ(result1, kBufSize) << "getrandom failed: " << strerror(errno);

  ssize_t result2 = getrandom(buf2.data(), buf2.size(), 0);
  ASSERT_EQ(result2, kBufSize) << "getrandom failed: " << strerror(errno);

  // It is mathematically possible that these assertions fail even though the
  // random number generator is working properly (the odds are 1 in 2^4096 and
  // 1 in 2^2048 respectively). If they do, rerunning the test should fix it.
  EXPECT_NE(buf1, buf2) << "Two random buffers should not be identical.";

  bool all_zero = std::ranges::all_of(buf1, [](char c) { return c == 0; });
  EXPECT_FALSE(all_zero) << "Random buffer should not be all zeros.";
}

TEST(GetRandomTest, HandlesSmallBuffer) {
  char buf[1];
  ssize_t result = getrandom(buf, sizeof(buf), 0);
  if (result == -1 && errno == ENOSYS) {
    GTEST_SKIP() << "getrandom() not supported on this platform.";
  }
  EXPECT_EQ(result, 1);
}

TEST(GetRandomTest, HandlesLargeBuffer) {
  const ssize_t kLargeBufSize = 1024 * 1024;  // 1MB
  std::vector<char> buf(kLargeBufSize);
  ssize_t total_read = 0;

  // getrandom can return fewer bytes than requested in some cases, e.g. if
  // interrupted, but for 1MB it should usually succeed in one call.
  while (total_read < kLargeBufSize) {
    ssize_t result =
        getrandom(buf.data() + total_read, kLargeBufSize - total_read, 0);
    if (result == -1) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == ENOSYS) {
        GTEST_SKIP() << "getrandom() not supported on this platform.";
      }
      FAIL() << "getrandom failed: " << strerror(errno);
    }
    total_read += result;
    if (result == 0) {
      break;
    }
  }
  EXPECT_EQ(total_read, kLargeBufSize);
}

TEST(GetRandomTest, RandomFlag) {
  char buf[16];
  ssize_t result = getrandom(buf, 16, GRND_RANDOM);
  if (result == -1) {
    if (errno == ENOSYS) {
      GTEST_SKIP() << "getrandom() not supported on this platform.";
    }
    FAIL() << "getrandom failed with unexpected error: " << strerror(errno);
  }
  EXPECT_EQ(result, 16);
}

TEST(GetRandomTest, NonBlocking) {
  char buf[16];
  ssize_t result = getrandom(buf, 16, GRND_NONBLOCK);
  if (result == -1) {
    if (errno == ENOSYS) {
      GTEST_SKIP() << "getrandom() not supported on this platform.";
    }
    if (errno == EAGAIN) {
      // Entropy pool not ready, this is a valid result for GRND_NONBLOCK.
      return;
    }
    FAIL() << "getrandom failed with unexpected error: " << strerror(errno);
  }
  EXPECT_EQ(result, 16);
}

TEST(GetRandomTest, HandlesLargeBufferNonBlocking) {
  const ssize_t kLargeBufSize = 1024 * 1024;  // 1MB
  std::vector<char> buf(kLargeBufSize);

  // An unfilled buffer result is more likely with GRND_RANDOM set.
  ssize_t result =
      getrandom(buf.data(), kLargeBufSize, GRND_RANDOM | GRND_NONBLOCK);
  if (result == -1) {
    if (errno == ENOSYS) {
      GTEST_SKIP() << "getrandom() not supported on this platform.";
    }
    if (errno == EAGAIN) {
      // Entropy pool not ready, this is a valid result for GRND_NONBLOCK.
      return;
    }
    FAIL() << "getrandom failed with unexpected error: " << strerror(errno);
  }
  EXPECT_GT(result, 0);
  EXPECT_LE(result, kLargeBufSize);
}

TEST(GetRandomTest, InvalidFlags) {
  unsigned flag = ~0U;
  char buf[16];
  ssize_t result = getrandom(buf, sizeof(buf), flag);
  EXPECT_EQ(result, -1);
  if (errno == ENOSYS) {
    GTEST_SKIP() << "getrandom() not supported on this platform.";
  }
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
