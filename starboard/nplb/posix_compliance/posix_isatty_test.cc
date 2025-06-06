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
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixIsattyTest : public ::testing::Test {
 protected:
  PosixIsattyTest()
      : original_stdin_fd_(-1),
        original_stdout_fd_(-1),
        original_stderr_fd_(-1),
        stdio_descriptors_cached_(false) {}

  void SetUp() override {
    errno = 0;

    stdio_descriptors_cached_ = false;
    original_stdin_fd_ = -1;
    original_stdout_fd_ = -1;
    original_stderr_fd_ = -1;
  }

  void TearDown() override {
    if (stdio_descriptors_cached_) {
      RestoreStdioFileNos();
    }
  }

  void CacheStdioFileNos() {
    original_stdin_fd_ = dup(STDIN_FILENO);
    original_stdout_fd_ = dup(STDOUT_FILENO);
    original_stderr_fd_ = dup(STDERR_FILENO);

    ASSERT_NE(-1, original_stdin_fd_)
        << "Failed to cache original STDIN_FILENO: " << strerror(errno);
    ASSERT_NE(-1, original_stdout_fd_)
        << "Failed to cache original STDOUT_FILENO: " << strerror(errno);
    ASSERT_NE(-1, original_stderr_fd_)
        << "Failed to cache original STDERR_FILENO: " << strerror(errno);

    stdio_descriptors_cached_ = true;
  }

  void RestoreStdioFileNos() {
    if (original_stdin_fd_ != -1) {
      dup2(original_stdin_fd_, STDIN_FILENO);
      close(original_stdin_fd_);
    }
    if (original_stdout_fd_ != -1) {
      dup2(original_stdout_fd_, STDOUT_FILENO);
      close(original_stdout_fd_);
    }
    if (original_stderr_fd_ != -1) {
      dup2(original_stderr_fd_, STDERR_FILENO);
      close(original_stderr_fd_);
    }
  }

 private:
  // Cache the original handles for the stdio descriptors to restore them after
  // a test.
  int original_stdin_fd_;
  int original_stdout_fd_;
  int original_stderr_fd_;
  bool stdio_descriptors_cached_;
};

// Tests that isatty() handles stdin correctly. When the test is run from a
// terminal, isatty(STDIN_FILENO) should return 1. It should return 0 in all
// other cases, e.g., running from a Github Action.
TEST_F(PosixIsattyTest, HandlesStdin) {
  int retval = isatty(STDIN_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  } else {
    ASSERT_EQ(retval, 1) << "isatty(STDIN_FILENO) returns " << retval
                         << ". Non-zero return values can only be 1.";
    SB_LOG(INFO) << "isatty(STDIN_FILENO) returns 1. This should only happen "
                    "when the test is run directly from a terminal.";
  }

  // Test that isatty(STDIN_FILENO) returns 0 when the input is piped.
  CacheStdioFileNos();
  int pipe_fds[2];

  ASSERT_EQ(pipe(pipe_fds), 0) << "Failed to create pipe: " << strerror(errno);
  ASSERT_NE(-1, dup2(pipe_fds[0], STDIN_FILENO))
      << "Failed to duplicate a pipe fd: " << strerror(errno);
  close(pipe_fds[0]);

  EXPECT_EQ(0, isatty(STDIN_FILENO));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  close(STDIN_FILENO);
}

// Tests that isatty() handles stdout correctly. When the test is run from a
// terminal, isatty(STDOUT_FILENO) should return 1. It should return 0 in all
// other cases, e.g., running from a Github Action.
TEST_F(PosixIsattyTest, HandlesStdout) {
  int retval = isatty(STDOUT_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  } else {
    ASSERT_EQ(retval, 1) << "isatty(STDOUT_FILENO) returns " << retval
                         << ". Non-zero return values can only be 1.";
    SB_LOG(INFO) << "isatty(STDOUT_FILENO) returns 1. This should only happen "
                    "when the test is run directly from a terminal.";
  }

  // Test that isatty(STDOUT_FILENO) returns 0 when the output is piped.
  CacheStdioFileNos();
  int pipe_fds[2];

  ASSERT_EQ(pipe(pipe_fds), 0) << "Failed to create pipe: " << strerror(errno);
  ASSERT_NE(-1, dup2(pipe_fds[1], STDOUT_FILENO))
      << "Failed to duplicate a pipe fd: " << strerror(errno);
  close(pipe_fds[1]);

  EXPECT_EQ(0, isatty(STDOUT_FILENO));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  close(STDOUT_FILENO);
}

// Tests that isatty() handles stderr correctly. When the test is run from a
// terminal, isatty(STDERR_FILENO) should return 1. It should return 0 in all
// other cases, e.g., running from a Github Action.
TEST_F(PosixIsattyTest, HandlesStderr) {
  int retval = isatty(STDERR_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  } else {
    ASSERT_EQ(retval, 1) << "isatty(STDERR_FILENO) returns " << retval
                         << ". Non-zero return values can only be 1.";
    SB_LOG(INFO) << "isatty(STDERR_FILENO) returns 1. This should only happen "
                    "when the test is run directly from a terminal.";
  }

  // Test that isatty(STDERR_FILENO) returns 0 when the output is piped.
  CacheStdioFileNos();
  int pipe_fds[2];

  ASSERT_EQ(pipe(pipe_fds), 0) << "Failed to create pipe: " << strerror(errno);
  ASSERT_NE(-1, dup2(pipe_fds[1], STDERR_FILENO))
      << "Failed to duplicate a pipe fd: " << strerror(errno);
  close(pipe_fds[1]);

  EXPECT_EQ(0, isatty(STDERR_FILENO));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
  close(STDERR_FILENO);
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
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDONLY);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);
  close(fd);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(EBADF, errno) << "Expected EBADF, got " << strerror(errno);
}

// Tests that isatty() sets ENOTTY on an open file.
TEST_F(PosixIsattyTest, HandlesOpenFile) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDONLY);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);

  EXPECT_FALSE(isatty(fd));
  EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);

  close(fd);
}

// Tests that isatty() recognizes a tty device as a tty.
TEST_F(PosixIsattyTest, HandlesTtyDevice) {
  int retval = isatty(STDIN_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
    GTEST_SKIP() << "isatty(STDIN_FILENO) returns 0. Skip the rest of the test "
                    "as the test isn't run from a terminal.";
  }

  ASSERT_EQ(retval, 1) << "isatty(STDIN_FILENO) returns invalid value"
                       << retval;

  int tty = open("/dev/tty", O_RDWR);
  ASSERT_NE(-1, tty) << "Failed to open /dev/tty: " << strerror(errno);
  ASSERT_TRUE(isatty(tty));

  close(tty);
}

// Tests that isatty() recognizes a duplicate of a tty device as a tty.
TEST_F(PosixIsattyTest, HandlesDuplicatesOfTtyDevices) {
  int retval = isatty(STDIN_FILENO);

  if (retval == 0) {
    EXPECT_EQ(ENOTTY, errno) << "Expected ENOTTY, got " << strerror(errno);
    GTEST_SKIP() << "isatty(STDIN_FILENO) returns 0. Skip the rest of the test "
                    "as the test isn't run from a terminal.";
  }

  ASSERT_EQ(retval, 1) << "isatty(STDIN_FILENO) returns " << retval;

  int tty = open("/dev/tty", O_RDWR);
  ASSERT_NE(-1, tty) << "Failed to open /dev/tty: " << strerror(errno);
  ASSERT_TRUE(isatty(tty));

  int tty_copy = dup(tty);
  ASSERT_NE(-1, tty_copy) << "Failed to create duplicate fd for /dev/tty: "
                          << strerror(errno);
  EXPECT_TRUE(isatty(tty_copy));

  close(tty);
  close(tty_copy);
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
  ASSERT_EQ(pipe(pipe_fds), 0) << "Failed to create pipe.";

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
