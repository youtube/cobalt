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

// The following scenario is not reliably testable for pipe():
// - ENFILE: This error indicates the system file table is full, which is
//   difficult to trigger from a user-space test without impacting the system
//   or requiring specific, non-standard system configurations.

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Test that pipe() successfully places two valid file descriptors into the
// fildes array.
TEST(PosixPipeTest, PipePlacesTwoValidFileDescriptorsIntoFildesArray) {
  int pipe_fds[2];
  EXPECT_EQ(pipe(pipe_fds), 0);
  EXPECT_GE(pipe_fds[0], 0);
  EXPECT_GE(pipe_fds[1], 0);
  EXPECT_EQ(close(pipe_fds[0]), 0);
  EXPECT_EQ(close(pipe_fds[1]), 0);
}

// Test that the FD_CLOEXEC flag is clear on both ends of the pipe.
TEST(PosixPipeTest, PipeLeavesClosedOnExecClearOnBothEndsOfPipe) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(flags_read_end, -1);
  EXPECT_FALSE(flags_read_end & FD_CLOEXEC);

  int flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(flags_write_end, -1);
  EXPECT_FALSE(flags_write_end & FD_CLOEXEC);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// Test that the FD_CLOFORK flag is clear on both ends of the pipe.
TEST(PosixPipeTest, PipeLeavesClosedOnForkClearOnBothEndsOfPipe) {
#ifdef FD_CLOFORK  // FD_CLOFORK was added in POSIX.1-2017 (Issue 8)
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(flags_read_end, -1);
  EXPECT_FALSE(flags_read_end & FD_CLOFORK);

  int flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(flags_write_end, -1);
  EXPECT_FALSE(flags_write_end & FD_CLOFORK);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
#else
  GTEST_SKIP() << "FD_CLOFORK is not defined on this platform";
#endif
}

// Test that the O_NONBLOCK flag is clear on both ends of the pipe.
TEST(PosixPipeTest, PipeLeavesNonBlockClearOnBothEndsOfPipe) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int flags_read_end = fcntl(pipe_fds[0], F_GETFL);
  EXPECT_NE(flags_read_end, -1);
  EXPECT_FALSE(flags_read_end & O_NONBLOCK);

  int flags_write_end = fcntl(pipe_fds[1], F_GETFL);
  EXPECT_NE(flags_write_end, -1);
  EXPECT_FALSE(flags_write_end & O_NONBLOCK);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// Test that data written to the pipe's write end can be read from its read end.
TEST(PosixPipeTest, DataWrittenToPipeCanBeRead) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  const char TEST_DATA[] = "Hello, POSIX Pipe!";
  const size_t DATA_SIZE = sizeof(TEST_DATA);  // Include null terminator
  char read_buffer[DATA_SIZE];

  // Write to the write end
  ssize_t bytes_written = write(pipe_fds[1], TEST_DATA, DATA_SIZE);
  EXPECT_EQ(bytes_written, DATA_SIZE);

  // Read from the read end
  ssize_t bytes_read = read(pipe_fds[0], read_buffer, DATA_SIZE);
  EXPECT_EQ(bytes_read, DATA_SIZE);
  EXPECT_STREQ(read_buffer, TEST_DATA);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// Test that pipe() returns -1 and sets errno to EMFILE when too many file
// descriptors are open.
TEST(PosixPipeTest, DISABLED_ReturnsEMFILEIfTooManyFileDescriptorsOpen) {
  // TODO: b/412648662 - If/when getrlimit() and setrimit() are added to the
  // hermetic build, use them to drop the maximum value that the system can
  // assign to new file descriptors for this process to just above the current
  // usage in order to easily force this scenario.
  FAIL() << "This test requires the ability to manipulate process resource "
         << "limits (rlimit) to reliably trigger EMFILE, which is not "
         << "currently available in this test environment.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
