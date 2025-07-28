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

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// A helper class that creates a temporary file and ensures it is unlinked
// when the object goes out of scope.
class ScopedTempFile {
 public:
  ScopedTempFile() : fd_(-1) {
    char temp_path_template[] = "/tmp/fdatasync_test_XXXXXX";
    fd_ = mkstemp(temp_path_template);
    if (fd_ != -1) {
      // The file is created with 0600 permissions, which is what we want.
      // We close it immediately so the test can open it with desired flags.
      close(fd_);
    }
    filename_ = temp_path_template;
  }

  ~ScopedTempFile() { unlink(filename_.c_str()); }

  const char* filename() const { return filename_.c_str(); }
  bool is_valid() const { return fd_ != -1; }

 private:
  std::string filename_;
  int fd_;
};

TEST(PosixFdatasyncTest, SunnyDay) {
  ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.is_valid());
  const char kData[] = "test data";
  const size_t kDataSize = sizeof(kData) - 1;

  int fd = open(temp_file.filename(), O_WRONLY);
  ASSERT_NE(fd, -1) << "Failed to open temp file: " << strerror(errno);

  ssize_t bytes_written = write(fd, kData, kDataSize);
  ASSERT_EQ(bytes_written, kDataSize);

  // fdatasync() should succeed.
  EXPECT_EQ(fdatasync(fd), 0);

  // Close and reopen to ensure we are not just reading from a dirty buffer.
  EXPECT_EQ(close(fd), 0);

  fd = open(temp_file.filename(), O_RDONLY);
  ASSERT_NE(fd, -1);

  char read_buffer[kDataSize];
  ssize_t bytes_read = read(fd, read_buffer, kDataSize);
  EXPECT_EQ(bytes_read, kDataSize);
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
  ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.is_valid());
  int fd = open(temp_file.filename(), O_WRONLY);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  // fdatasync() on a closed file descriptor should also fail with EBADF.
  errno = 0;
  EXPECT_EQ(fdatasync(fd), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFdatasyncTest, ReadOnlyFileDescriptor) {
  ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.is_valid());
  int fd = open(temp_file.filename(), O_RDONLY);
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

TEST(PosixFdatasyncTest, Pipe) {
  // The POSIX standard says fdatasync() can return EINVAL if the
  // implementation does not support synchronized I/O for the file type.
  // The behavior on a pipe is not strictly defined. Some systems may return
  // EINVAL, others may succeed (as a no-op). This test accepts either outcome.
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  errno = 0;
  int result_read = fdatasync(pipe_fds[0]);
  if (result_read == -1) {
    EXPECT_EQ(errno, EINVAL);
  } else {
    EXPECT_EQ(result_read, 0);
  }

  errno = 0;
  int result_write = fdatasync(pipe_fds[1]);
  if (result_write == -1) {
    EXPECT_EQ(errno, EINVAL);
  } else {
    EXPECT_EQ(result_write, 0);
  }

  EXPECT_EQ(close(pipe_fds[0]), 0);
  EXPECT_EQ(close(pipe_fds[1]), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
