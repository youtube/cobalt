// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// flush() is otherwise tested in PosixFileWriteTest.

/*
  The following fsync() error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - [EINTR]: Interruption by a signal, which is racy and hard to test.
  - [EIO]: A low-level I/O error occurred on the filesystem.
*/
#include <sys/socket.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFileSyncTest, InvalidFileErrors) {
  const int result = fsync(-1);
  EXPECT_NE(result, 0) << "fsync with invalid file descriptor should fail";
  EXPECT_EQ(errno, EBADF) << "fsync with invalid file descriptor should set "
                             "errno to EBADF, but got "
                          << strerror(errno);
}

TEST(PosixFileSyncTest, SuccessOnValidFile) {
  ScopedRandomFile random_file;
  int fd = open(random_file.filename().c_str(), O_WRONLY);
  ASSERT_NE(fd, -1);

  const char* data = "test";
  ASSERT_EQ(write(fd, data, 4), 4);

  int result = fsync(fd);
  EXPECT_EQ(result, 0) << "fsync failed on a valid file: " << strerror(errno);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixFileSyncTest, FailsOnUnsupportedFileType) {
  int sockets[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  errno = 0;
  int result = fsync(sockets[0]);
  // The specific error can vary. EINVAL is specified by POSIX, but some
  // systems might return EBADF if fsync is not supported on the object type.
  EXPECT_EQ(result, -1);
  EXPECT_TRUE(errno == EINVAL || errno == EBADF)
      << "Expected EINVAL or EBADF, but got " << errno << " ("
      << strerror(errno) << ")";

  EXPECT_EQ(close(sockets[0]), 0);
  EXPECT_EQ(close(sockets[1]), 0);
}

}  // namespace
}  // namespace nplb
