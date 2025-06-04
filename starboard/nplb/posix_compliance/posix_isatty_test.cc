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

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixIsattyTest : public ::testing::Test {
 protected:
  void SetUp() override { errno = 0; }
};

// Tests that isatty() handles stdin correctly. When the test is run from a
// terminal, isatty(STDIN_FILENO) should return 1. It should return 0 in all
// other cases, e.g., running from a Github Action.
TEST_F(PosixIsattyTest, HandlesStdin) {
  int retval = isatty(STDIN_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  } else {
    ASSERT_EQ(retval, 1) << "isatty(STD_FILENO) returns " << retval
                         << ". Non-zero return values can only be 1.";
    GTEST_SKIP() << "isatty(STD_FILENO) returns 1. This should only happen "
                    "when the test is run directly from a terminal.";
  }
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
  const char* kTestFileName = "isatty_test_closed.txt";

  int fd = open(kTestFileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);
  close(fd);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(EBADF, errno) << "Expected EBADF, got " << strerror(errno);

  remove(kTestFileName);
}

// Tests that isatty() sets ENOTTY on an open file.
TEST_F(PosixIsattyTest, HandlesOpenFile) {
  const char* kTestFileName = "isatty_test_open.txt";

  int fd = open(kTestFileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);

  close(fd);
  remove(kTestFileName);
}

// Tests that isatty() doesn't recognize non-tty devices as ttys.
TEST_F(PosixIsattyTest, HandlesNonTtyDevices) {
  int fd = open("/dev/null", O_RDWR);
  ASSERT_NE(-1, fd) << "Failed to open /dev/null: " << strerror(errno);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  close(fd);

  errno = 0;

  fd = open("/dev/zero", O_RDONLY);
  ASSERT_NE(-1, fd) << "Failed to open /dev/zero: " << strerror(errno);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  close(fd);
}

// Tests that isatty() does not recognize a duplicate of a non tty
// device as a tty.
TEST_F(PosixIsattyTest, HandlesDuplicatesOfNonTtyDevices) {
  int fd = open("/dev/null", O_RDWR);
  ASSERT_NE(-1, fd) << "Failed to open /dev/null: " << strerror(errno);
  ASSERT_FALSE(isatty(fd));
  ASSERT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);

  errno = 0;

  int fd_copy = dup(fd);
  ASSERT_NE(-1, fd_copy) << "Failed to create duplicate fd: "
                         << strerror(errno);
  EXPECT_FALSE(isatty(fd_copy));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);

  close(fd);
  close(fd_copy);
}

// Tests that isatty() doesn't recognize a pipe as a tty.
TEST_F(PosixIsattyTest, HandlesPipes) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  EXPECT_FALSE(isatty(pipe_fds[0]));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  errno = 0;

  EXPECT_FALSE(isatty(pipe_fds[1]));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
