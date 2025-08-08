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

#include <bitset>
#include <string>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace starboard {
namespace nplb {
namespace {

class PosixFcntlTest : public ::testing::Test {
 protected:
  PosixFcntlTest() = default;

  void SetUp() override { errno = 0; }
};

// Tests that F_DUPFD copies the same status flags on the duplicate file
// descriptor.
TEST_F(PosixFcntlTest, DuplicateFileDescriptorCopiesStatusFlags) {
  ScopedRandomFile random_file(64);
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(-1, fd) << "Failed to open test file: " << strerror(errno);

  // Duplicate the file descriptor.
  int new_fd = fcntl(fd, F_DUPFD, 0);
  ASSERT_NE(new_fd, -1) << "fcntl failed: " << strerror(errno);
  ASSERT_NE(new_fd, fd);

  // Verify that the new file descriptor has the same file status flags.
  int original_flags = fcntl(fd, F_GETFL);
  ASSERT_NE(original_flags, -1) << "fcntl failed: " << strerror(errno);
  int new_flags = fcntl(new_fd, F_GETFL);
  ASSERT_NE(new_flags, -1) << "fcntl failed: " << strerror(errno);
  ASSERT_EQ(original_flags, new_flags);

  close(fd);
  close(new_fd);
}

// Tests that F_GETFD returns empty flags on a file decriptor that never had
// flags set with F_SETFD.
TEST_F(PosixFcntlTest, GetFdFlagsDoesntDetectNonExistentFlags) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  // Get the file descriptor flags.
  int flags = fcntl(fd, F_GETFD);
  ASSERT_NE(flags, -1) << "fcntl failed: " << strerror(errno);
  // Check that flags that were never set are not present.
  ASSERT_NE(flags & FD_CLOEXEC, FD_CLOEXEC);

  close(fd);
}

// Tests that F_SETFD sets the FD_CLOEXEC flag on a file descriptor.
TEST_F(PosixFcntlTest, SetFileDescriptorFlags) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  int result = fcntl(fd, F_GETFD);
  ASSERT_NE(result, -1) << "fcntl failed: " << strerror(errno);

  // Set the FD_CLOEXEC flag.
  result = fcntl(fd, F_SETFD, FD_CLOEXEC);
  ASSERT_NE(result, -1) << "fcntl failed: " << strerror(errno);

  // Verify that the FD_CLOEXEC flag is set.
  int updated_flags = fcntl(fd, F_GETFD);
  ASSERT_NE(updated_flags, -1) << "fcntl failed: " << strerror(errno);
  ASSERT_EQ(updated_flags & FD_CLOEXEC, FD_CLOEXEC)
      << "Updated flags " << std::bitset<32>(updated_flags)
      << " doesn't match FC_CLOEXEC ()" << std::bitset<32>(FD_CLOEXEC) << ")";

  close(fd);
}

// Verify that the F_GETFL and F_SETFL commands properly get and set file status
// flags, respectively.
TEST_F(PosixFcntlTest, GetAndSetFileStatusFlags) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  // Verify that the O_RDWR flag specified in open() is set in the file status
  // flags.
  int initial_flags = fcntl(fd, F_GETFL);
  ASSERT_NE(initial_flags, -1) << "fcntl failed: " << strerror(errno);
  ASSERT_EQ(initial_flags & O_ACCMODE, O_RDWR)
      << "Status flag " << (initial_flags & O_ACCMODE) << " is not O_RDWR ("
      << O_RDWR << ")";

  int result = fcntl(fd, F_SETFL, initial_flags | O_APPEND);
  ASSERT_NE(result, -1) << "Failed to set status flag O_APPEND: "
                        << strerror(errno);

  // Verify that the O_APPEND flag is set.
  int updated_flags = fcntl(fd, F_GETFL);
  ASSERT_NE(updated_flags, -1) << "fcntl failed: " << strerror(errno);
  ASSERT_EQ(updated_flags & O_APPEND, O_APPEND)
      << "Status flag O_APPEND (" << std::bitset<32>(O_APPEND)
      << ") is not present in file status flags: "
      << std::bitset<32>(updated_flags);
  ASSERT_EQ(initial_flags & O_ACCMODE, O_RDWR)
      << "Status flag O_RDWR (" << std::bitset<32>(O_RDWR)
      << ") is not present in file status flags: "
      << std::bitset<32>(updated_flags);

  close(fd);
}

// TODO: Properly test file locking behavior with F_GETLK and F_SETLK.
// Tests that F_SETLK executes without error.
TEST_F(PosixFcntlTest, SetLockWithoutError) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  // Create a lock.
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  // Set the lock.
  int result = fcntl(fd, F_SETLK, &lock);
  ASSERT_NE(result, -1) << "fcntl failed: " << strerror(errno);

  // Unlock the file.
  lock.l_type = F_UNLCK;
  result = fcntl(fd, F_SETLK, &lock);
  ASSERT_NE(result, -1) << "fcntl failed: " << strerror(errno);

  close(fd);
}

// Tests that fcntl() with an invalid command fails and sets EINVAL.
TEST_F(PosixFcntlTest, RejectInvalidCommand) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  int result = fcntl(fd, -1, 0);
  ASSERT_EQ(result, -1) << "Expected fcntl to fail, instead got return value: "
                        << result;
  ASSERT_EQ(EINVAL, errno) << "Expected EINVAL, got " << strerror(errno);

  close(fd);
}

// Tests that fcntl() with F_DUPFD with an arg of INT_MAX fails and sets EINVAL.
TEST_F(PosixFcntlTest, RejectInvalidFileDescriptorTooHigh) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  int fd = open(filename.c_str(), O_RDWR);
  ASSERT_NE(fd, -1) << "Failed to open test file: " << strerror(errno);

  int result = fcntl(fd, F_DUPFD, INT_MAX);
  ASSERT_EQ(result, -1) << "Expected fcntl to fail, instead got return value: "
                        << result;
  ASSERT_EQ(EINVAL, errno) << "Expected EINVAL, got " << strerror(errno);

  close(fd);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
