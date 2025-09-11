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

// Some failure cases from
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/dup.html are not
// (yet) tested:
// - dup2() should fail and set errno to EINTR if it is interrupted by a signal,
//   but it's infeasible to reliably reproduce such an interruption.
// - dup2() should fail and set errno to EBADF if fildes2 >= {OPEN_MAX}, but it
//   appears that POSIX systems are not required to define this macro.
// - dup2() may fail if an I/O error occurs while attempting to close fildes2,
//   but we should not enforce optional behaviors in our compliance tests.
// - dup() should fail and set errno to EMFILE when all descriptors available
//   to the system are already open. TODO: b/412648662 - If/when getrlimit() and
//   setrimit() are added to the hermetic build, use them to drop the maximum
//   value that the system can assign to new file descriptors to just above the
//   current usage in order to easily force this scenario.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

constexpr int k8ByteFileLength = 8;

TEST(PosixFileDescriptorDuplicateTest, DupDuplicatesValidOpenFd) {
  // Given an initial file descriptor pointing to the beginning of a file.
  ScopedRandomFile random_file(k8ByteFileLength, ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();
  int fd = open(filename.c_str(), O_RDONLY);
  ASSERT_GE(fd, 0) << "open: " << strerror(errno);
  int64_t fd_offset = lseek(fd, 0, SEEK_SET);
  ASSERT_EQ(fd_offset, 0) << "lseek: " << strerror(errno);

  // When the offset is changed using a duplicate file descriptor.
  int new_fd = dup(fd);
  ASSERT_EQ(lseek(new_fd, 1, SEEK_SET), 1) << "lseek: " << strerror(errno);

  // Then the change in offset is reflected in the initial file descriptor.
  fd_offset = lseek(fd, 0, SEEK_CUR);
  EXPECT_EQ(fd_offset, 1);

  EXPECT_EQ(close(fd), 0) << "close: " << strerror(errno);
  EXPECT_EQ(close(new_fd), 0) << "close: " << strerror(errno);
}

TEST(PosixFileDescriptorDuplicateTest, Dup2DuplicatesValidOpenFd) {
  // Given two file descriptors, x and y, pointing to the beginnings of
  // different files.
  ScopedRandomFile random_file_x(k8ByteFileLength, ScopedRandomFile::kCreate);
  const std::string& filename_x = random_file_x.filename();
  int fd_x = open(filename_x.c_str(), O_RDONLY);
  ASSERT_GE(fd_x, 0) << "open: " << strerror(errno);
  int64_t fd_x_offset = lseek(fd_x, 0, SEEK_SET);
  ASSERT_EQ(fd_x_offset, 0) << "lseek: " << strerror(errno);

  ScopedRandomFile random_file_y(k8ByteFileLength, ScopedRandomFile::kCreate);
  const std::string& filename_y = random_file_y.filename();
  int fd_y = open(filename_y.c_str(), O_RDONLY);
  ASSERT_GE(fd_y, 0) << "open: " << strerror(errno);
  ASSERT_EQ(lseek(fd_y, 0, SEEK_SET), 0) << "lseek: " << strerror(errno);

  // When the offset is changed for fd y.
  ASSERT_EQ(lseek(fd_y, 1, SEEK_SET), 1) << "lseek: " << strerror(errno);

  // Then the change in offset is not reflected in fd x, because the file
  // descriptors are not duplicates.
  fd_x_offset = lseek(fd_x, 0, SEEK_CUR);
  ASSERT_EQ(fd_x_offset, 0);

  // But when fd y is made to be a duplicate of fd x and then the offset is
  // changed for fd y.
  ASSERT_EQ(dup2(fd_x, fd_y), fd_y);
  ASSERT_EQ(lseek(fd_y, 1, SEEK_SET), 1) << "lseek: " << strerror(errno);

  // Then the change in offset is reflected in fd x.
  fd_x_offset = lseek(fd_x, 0, SEEK_CUR);
  EXPECT_EQ(fd_x_offset, 1);

  EXPECT_EQ(close(fd_x), 0) << "close: " << strerror(errno);
  EXPECT_EQ(close(fd_y), 0) << "close: " << strerror(errno);
}

TEST(PosixFileDescriptorDuplicateTest, DupNegativeIntFails) {
  int new_fd = dup(-1);

  EXPECT_EQ(new_fd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFileDescriptorDuplicateTest, Dup2NegativeIntASOldFdFails) {
  int new_fd = dup2(-1, STDERR_FILENO);

  EXPECT_EQ(new_fd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFileDescriptorDuplicateTest, DupClosedFdFails) {
  ScopedRandomFile random_file(k8ByteFileLength, ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();
  int fd = open(filename.c_str(), O_RDONLY);
  ASSERT_GE(fd, 0) << "open: " << strerror(errno);
  EXPECT_EQ(close(fd), 0) << "close: " << strerror(errno);

  int new_fd = dup(fd);  // fd no longer represents a file descriptor

  EXPECT_EQ(new_fd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFileDescriptorDuplicateTest, Dup2ClosedOldFdFails) {
  ScopedRandomFile random_file(k8ByteFileLength, ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();
  int fd = open(filename.c_str(), O_RDONLY);
  ASSERT_GE(fd, 0) << "open: " << strerror(errno);
  EXPECT_EQ(close(fd), 0) << "close: " << strerror(errno);

  // fd no longer represents a file descriptor
  int new_fd = dup2(fd, STDERR_FILENO);

  EXPECT_EQ(new_fd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFileDescriptorDuplicateTest, Dup2NegativeIntASNewFdFails) {
  int new_fd = dup2(STDOUT_FILENO, -1);

  EXPECT_EQ(new_fd, -1);
  EXPECT_EQ(errno, EBADF);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
