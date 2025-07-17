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
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#ifdef HOST_NAME_MAX
constexpr int kMaxHostNameSize = HOST_NAME_MAX + 1;  // +1 for null terminator.
#else
constexpr int kMaxHostNameSize = 256;  // 255 + 1 for null terminator.
#endif

class PosixGetHostnameTest : public ::testing::Test {
 protected:
  PosixGetHostnameTest() = default;

  void SetUp() override { errno = 0; }
};

// Basic successful call with a sufficiently large buffer.
TEST_F(PosixGetHostnameTest, SucceedsBasicCall) {
  std::vector<char> buf(kMaxHostNameSize);
  int result = gethostname(buf.data(), buf.size());

  ASSERT_EQ(0, result) << "gethostname() failed with error: " << strerror(errno)
                       << ".";
  EXPECT_EQ(0, errno) << "errno was set to " << errno << " (" << strerror(errno)
                      << ") after a successful gethostname call.";
}

// Correctly handles a buffer exactly the size of the hostname.
TEST_F(PosixGetHostnameTest, SucceedsWithMimimumValidBufferLength) {
  std::vector<char> temp_buf(kMaxHostNameSize);
  ASSERT_EQ(0, gethostname(temp_buf.data(), temp_buf.size()))
      << "gethostname() failed: " << strerror(errno);
  size_t hostname_len = strlen(temp_buf.data());

  std::vector<char> buf(hostname_len + 1);
  int result = gethostname(buf.data(), buf.size());

  ASSERT_EQ(0, result) << "gethostname() with the exact buffer size ("
                       << hostname_len << ") failed: " << strerror(errno);
  EXPECT_EQ(0, errno) << "errno was set to " << errno << " (" << strerror(errno)
                      << ") after a successful call.";

  EXPECT_STREQ(temp_buf.data(), buf.data()) << "Hostname mismatch.";
  EXPECT_EQ('\0', buf[hostname_len]) << "Hostname is not null-terminated.";
}

TEST_F(PosixGetHostnameTest, MultipleCallsReturnConsistentData) {
  std::vector<char> buf1(kMaxHostNameSize);
  std::vector<char> buf2(kMaxHostNameSize);

  ASSERT_EQ(0, gethostname(buf1.data(), buf1.size()))
      << "First gethostname() call failed: " << strerror(errno);

  ASSERT_EQ(0, gethostname(buf2.data(), buf2.size()))
      << "Second gethostname() call failed: " << strerror(errno);

  EXPECT_STREQ(buf1.data(), buf2.data())
      << "Hostname inconsistent across calls.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
