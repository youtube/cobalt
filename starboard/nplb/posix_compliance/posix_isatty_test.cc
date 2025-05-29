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

// Tests that isatty() doesn't recognize devices as ttys.
TEST_F(PosixIsattyTest, HandlesDevices) {
  int fd = open("/dev/null", O_RDWR);
  ASSERT_NE(-1, fd) << "Failed to open /dev/null";
  EXPECT_FALSE(isatty(fd));
  EXPECT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);
  close(fd);
  errno = 0;

  fd = open("/dev/zero", O_RDONLY);
  ASSERT_NE(-1, fd) << "Failed to open /dev/zero";
  EXPECT_FALSE(isatty(fd));
  EXPECT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);
  close(fd);
}

// Tests that isatty() recognizes a duplicate of a valid file descriptor as a
// tty.
TEST_F(PosixIsattyTest, HandlesValidDuplicateFd) {
  ASSERT_TRUE(isatty(STDIN_FILENO));
  ASSERT_EQ(0, errno) << "errno was set: " << strerror(errno);

  int fd = dup(STDIN_FILENO);
  EXPECT_TRUE(isatty(fd));
  EXPECT_EQ(0, errno) << "errno was set: " << strerror(errno);
}

// Tests that isatty() does not recognize a duplicate of an invalid file
// descriptor as a tty.
TEST_F(PosixIsattyTest, HandlesInvalidDuplicateFd) {
  int fd = open("/dev/null", O_RDWR);
  ASSERT_NE(-1, fd) << "Failed to open /dev/null";
  ASSERT_FALSE(isatty(fd));
  ASSERT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);
  errno = 0;

  int fd_copy = dup(fd);
  EXPECT_FALSE(isatty(fd_copy));
  EXPECT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);

  close(fd);
}

// Tests that isatty() doesn't recognize a pipe as a tty.
TEST_F(PosixIsattyTest, HandlesPipes) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  EXPECT_FALSE(isatty(pipe_fds[0]));
  EXPECT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);
  errno = 0;

  EXPECT_FALSE(isatty(pipe_fds[1]));
  EXPECT_TRUE(errno == 0 || errno == ENOTTY)
      << "Expected 0 or ENOTTY, got " << strerror(errno);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
