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
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixIsattyTest : public ::testing::Test {
 protected:
  void SetUp() override { errno = 0; }
};

// Tests that isatty() recognizes stdin as a tty.
TEST_F(PosixIsattyTest, HandlesStdin) {
  EXPECT_TRUE(isatty(STDIN_FILENO));
  EXPECT_EQ(0, errno) << "errno was set: " << strerror(errno);
}

// Tests that isatty() recognizes stdout as a tty.
TEST_F(PosixIsattyTest, HandlesStdout) {
  EXPECT_TRUE(isatty(STDOUT_FILENO));
  EXPECT_EQ(0, errno) << "errno was set: " << strerror(errno);
}

// Tests that isatty() recognizes stderr as a tty.
TEST_F(PosixIsattyTest, HandlesStderr) {
  EXPECT_TRUE(isatty(STDERR_FILENO));
  EXPECT_EQ(0, errno) << "errno was set: " << strerror(errno);
}

// Tests that isatty() does not recognize invalid file descriptors as a tty.
TEST_F(PosixIsattyTest, HandlesInvalidFd) {
  int invalid_fd = -1;

  EXPECT_FALSE(isatty(invalid_fd));
  EXPECT_EQ(EBADF, errno) << "Expected EBADF, got " << strerror(errno);

  invalid_fd = INT_MAX;

  EXPECT_FALSE(isatty(invalid_fd));
  EXPECT_EQ(EBADF, errno) << "Expected EBADF, got " << strerror(errno);
}

// Tests that isatty() does not recognize closed file descriptors as a tty.
TEST_F(PosixIsattyTest, HandlesClosedFd) {
  const char* kTestFileName = "isatty_test.txt";

  int fd = open(kTestFileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);
  close(fd);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(EBADF, errno) << "Expected EBADF, got " << strerror(errno);

  remove(kTestFileName);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
