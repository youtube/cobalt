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
#include <sys/ioctl.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Test cases for general ioctl errors that aren't specific to a particular
// operation.
TEST(PosixIoctlTest, InvalidFileDescriptor) {
  int bytes_available = 0;
  EXPECT_EQ(ioctl(-1, FIONREAD, &bytes_available), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixIoctlTest, NullPointer) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  EXPECT_EQ(ioctl(pipe_fds[0], FIONREAD, nullptr), -1);
  EXPECT_EQ(errno, EFAULT);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// Test cases for FIONREAD ioctl operation. As of C27, this is the only ioctl
// operation used in Cobalt.
TEST(PosixIoctlTest, FionreadEmptyPipe) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int bytes_available = -1;
  EXPECT_EQ(ioctl(pipe_fds[0], FIONREAD, &bytes_available), 0);
  EXPECT_EQ(bytes_available, 0);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixIoctlTest, FionreadPipeWithData) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  constexpr char kTestData[] = "Hello, FIONREAD!";
  constexpr size_t kTestDataSize = sizeof(kTestData);

  ASSERT_EQ(write(pipe_fds[1], kTestData, kTestDataSize),
            static_cast<ssize_t>(kTestDataSize));

  int bytes_available = -1;
  EXPECT_EQ(ioctl(pipe_fds[0], FIONREAD, &bytes_available), 0);
  EXPECT_EQ(bytes_available, static_cast<int>(kTestDataSize));

  // Read a portion of the data
  char read_buffer[5];
  constexpr size_t kReadSize = sizeof(read_buffer);
  ASSERT_EQ(read(pipe_fds[0], read_buffer, kReadSize),
            static_cast<ssize_t>(kReadSize));

  EXPECT_EQ(ioctl(pipe_fds[0], FIONREAD, &bytes_available), 0);
  EXPECT_EQ(bytes_available, static_cast<int>(kTestDataSize - kReadSize));

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

}  // namespace
}  // namespace nplb
