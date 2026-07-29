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
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFdatasyncTest, SunnyDay) {
  ScopedRandomFile temp_file(0, ScopedRandomFile::kCreate);
  ASSERT_FALSE(temp_file.filename().empty());
  const char kData[] = "test data";
  const size_t kDataSize = sizeof(kData) - 1;

  int fd = open(temp_file.filename().c_str(), O_WRONLY);
  ASSERT_NE(fd, -1) << "Failed to open temp file: " << strerror(errno);

  ssize_t bytes_written = write(fd, kData, kDataSize);
  ASSERT_EQ(bytes_written, static_cast<ssize_t>(kDataSize));

  // fdatasync() should succeed.
  EXPECT_EQ(fdatasync(fd), 0);

  // Close and reopen to ensure we are not just reading from a dirty buffer.
  EXPECT_EQ(close(fd), 0);

  fd = open(temp_file.filename().c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);

  char read_buffer[kDataSize];
  ssize_t bytes_read = read(fd, read_buffer, kDataSize);
  EXPECT_EQ(bytes_read, static_cast<ssize_t>(kDataSize));
  EXPECT_EQ(memcmp(kData, read_buffer, kDataSize), 0);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixFdatasyncTest, FailureInvalidFileDescriptor) {
  // fdatasync() on an invalid file descriptor should fail with EBADF.
  errno = 0;
  EXPECT_EQ(fdatasync(-1), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFdatasyncTest, FailureClosedFileDescriptor) {
  ScopedRandomFile temp_file(0, ScopedRandomFile::kCreate);
  ASSERT_FALSE(temp_file.filename().empty());
  int fd = open(temp_file.filename().c_str(), O_WRONLY);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  // fdatasync() on a closed file descriptor should also fail with EBADF.
  errno = 0;
  EXPECT_EQ(fdatasync(fd), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFdatasyncTest, ReadOnlyFileDescriptor) {
  ScopedRandomFile temp_file(0, ScopedRandomFile::kCreate);
  ASSERT_FALSE(temp_file.filename().empty());
  int fd = open(temp_file.filename().c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);

  // The POSIX standard for fsync (which fdatasync is based on) states that
  // calling it on a file descriptor opened for read-only is not guaranteed
  // to succeed. Some systems may return EINVAL, others may succeed (as a
  // no-op). We accept either outcome.
  errno = 0;
  int result = fdatasync(fd);
  if (result == -1) {
    EXPECT_EQ(errno, EINVAL);
  } else {
    EXPECT_EQ(result, 0);
  }

  EXPECT_EQ(close(fd), 0);
}

}  // namespace
}  // namespace nplb
