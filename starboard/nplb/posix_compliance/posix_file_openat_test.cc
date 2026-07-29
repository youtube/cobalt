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
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Test basic openat with AT_FDCWD and absolute path (behaves like open).
TEST(PosixFileOpenatTest, AtFdcwdAbsolutePath) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  int fd = openat(AT_FDCWD, filename.c_str(), O_RDONLY);
  EXPECT_GE(fd, 0);
  if (fd >= 0) {
    close(fd);
  }
}

// Test basic openat with directory fd and relative path.
TEST(PosixFileOpenatTest, DirFdRelativePath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  const std::string filename = "test_file.txt";
  int fd =
      openat(dir_fd, filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  EXPECT_GE(fd, 0);

  if (fd >= 0) {
    close(fd);

    // Verify file was created in the correct directory
    std::string full_path = temp_dir.path() + kSbFileSepString + filename;
    struct stat file_info;
    EXPECT_EQ(stat(full_path.c_str(), &file_info), 0);
  }

  close(dir_fd);
}

// Test that absolute path ignores the dirfd.
TEST(PosixFileOpenatTest, AbsolutePathIgnoresDirFd) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  // Open the temp dir to get a valid dirfd
  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  // Create another file elsewhere
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& absolute_path = random_file.filename();

  // openat with valid dirfd but absolute path should open the absolute path
  // file, not something relative to dirfd.
  int fd = openat(dir_fd, absolute_path.c_str(), O_RDONLY);
  EXPECT_GE(fd, 0);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
}

// EBADF: path is relative and fd is not AT_FDCWD and not a valid open file
// descriptor.
TEST(PosixFileOpenatTest, FailsBadFd) {
  errno = 0;
  int fd = openat(-1, "relative_path.txt", O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EBADF);

  errno = 0;
  fd = openat(9999, "relative_path.txt", O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EBADF);
}

// ENOTDIR: fd is valid but not a directory, and path is relative.
TEST(PosixFileOpenatTest, FailsNotADirectory) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  int file_fd = open(random_file.filename().c_str(), O_RDONLY);
  ASSERT_GE(file_fd, 0);

  errno = 0;
  int fd = openat(file_fd, "relative_path.txt", O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOTDIR);
  if (fd >= 0) {
    close(fd);
  }

  close(file_fd);
}

// ENOENT: Path does not exist.
TEST(PosixFileOpenatTest, FailsNoEntity) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  errno = 0;
  int fd = openat(dir_fd, "non_existent_file.txt", O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOENT);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
}

// EEXIST: O_CREAT and O_EXCL are set, and the named file exists.
TEST(PosixFileOpenatTest, FailsAlreadyExists) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  const std::string filename = "existing_file.txt";
  int fd =
      openat(dir_fd, filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_GE(fd, 0);
  close(fd);

  errno = 0;
  fd = openat(dir_fd, filename.c_str(), O_CREAT | O_EXCL | O_RDWR,
              S_IRUSR | S_IWUSR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EEXIST);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
}

// EISDIR: The named file is a directory and oflag includes O_WRONLY or O_RDWR,
// or includes O_CREAT without O_DIRECTORY.
TEST(PosixFileOpenatTest, FailsIsDir) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  errno = 0;
  int fd = openat(dir_fd, ".", O_WRONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EISDIR);
  if (fd >= 0) {
    close(fd);
  }

  errno = 0;
  fd = openat(dir_fd, ".", O_RDWR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EISDIR);
  if (fd >= 0) {
    close(fd);
  }

  errno = 0;
  fd = openat(dir_fd, ".", O_CREAT | O_RDWR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EISDIR);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
}

// EACCES: Search permission denied on a component of the path prefix.
TEST(PosixFileOpenatTest, FailsNoAccessSearch) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string sub_dir_path =
      temp_dir.path() + kSbFileSepString + "no_access_sub";
  ASSERT_EQ(mkdir(sub_dir_path.c_str(), kUserRwx), 0);

  std::string file_path = sub_dir_path + kSbFileSepString + "test.txt";
  int fd = open(file_path.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  ASSERT_GE(fd, 0);
  close(fd);

  // Remove read and search permissions from the subdirectory.
  ASSERT_EQ(chmod(sub_dir_path.c_str(), S_IWUSR), 0);

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  std::string relative_path =
      "no_access_sub" + std::string(kSbFileSepString) + "test.txt";
  errno = 0;
  fd = openat(dir_fd, relative_path.c_str(), O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EACCES);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);

  // Restore permissions to allow ScopedTempDir to clean up.
  chmod(sub_dir_path.c_str(), kUserRwx);
}

// EACCES: File does not exist and write permission is denied on the parent
// directory.
TEST(PosixFileOpenatTest, FailsNoAccessWrite) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  // Remove write permission from the directory.
  ASSERT_EQ(chmod(temp_dir.path().c_str(), S_IRUSR | S_IXUSR), 0);

  errno = 0;
  int fd = openat(dir_fd, "test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EACCES);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
  chmod(temp_dir.path().c_str(), kUserRwx);
}

// ENAMETOOLONG: Pathname too long.
TEST(PosixFileOpenatTest, FailsNameTooLong) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  std::string long_name(kSbFileMaxPath + 1, 'a');

  errno = 0;
  int fd = openat(dir_fd, long_name.c_str(), O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);
}

// ELOOP: Too many symbolic links.
TEST(PosixFileOpenatTest, FailsSymlinkLoop) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string path_a = temp_dir.path() + kSbFileSepString + "loop_a";
  std::string path_b = temp_dir.path() + kSbFileSepString + "loop_b";

  // Create a circular dependency: a -> b and b -> a.
  ASSERT_EQ(symlink(path_b.c_str(), path_a.c_str()), 0);
  ASSERT_EQ(symlink(path_a.c_str(), path_b.c_str()), 0);

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  errno = 0;
  int fd = openat(dir_fd, "loop_a", O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ELOOP);
  if (fd >= 0) {
    close(fd);
  }

  close(dir_fd);

  unlink(path_a.c_str());
  unlink(path_b.c_str());
}

}  // namespace
}  // namespace nplb
