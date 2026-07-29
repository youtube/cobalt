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

/*
  The following close() error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - [EINTR]: Interruption by a signal, which is racy and hard to test.
  - [EINPROGRESS]: Asynchronous close continuation after signal interruption.
  - [EIO]: A low-level I/O error occurred on the filesystem.
*/

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFileCloseTest, CloseInvalidFails) {
  constexpr int kInvalidFD = -1;

  const int result = close(kInvalidFD);
  EXPECT_NE(result, 0) << "close() with an invalid fd should not succeed";
  constexpr int kExpectedFailure = -1;
  EXPECT_EQ(result, kExpectedFailure)
      << "close() with an invalid fd should "
      << "return " << kExpectedFailure << " but returned " << result;
  EXPECT_EQ(errno, EBADF) << "Expected errno to be EBADF (" << EBADF
                          << ") after attempting to close(" << kInvalidFD
                          << ") "
                          << "for an invalid file descriptor, but got " << errno
                          << " (" << strerror(errno) << ").";
}

TEST(PosixFileCloseTest, SuccessOnValidFileDescriptor) {
  ScopedRandomFile random_file;
  int fd = open(random_file.filename().c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);

  int result = close(fd);
  EXPECT_EQ(result, 0) << "close() failed on a valid file descriptor: "
                       << strerror(errno);
}

TEST(PosixFileCloseTest, DoubleCloseFails) {
  ScopedRandomFile random_file;
  int fd = open(random_file.filename().c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);

  // First close should succeed.
  int result = close(fd);
  ASSERT_EQ(result, 0) << "close failed on a valid file descriptor : "
                       << strerror(errno);

  // Second close on the same fd should fail.
  errno = 0;
  result = close(fd);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EBADF);
}

}  // namespace
}  // namespace nplb
