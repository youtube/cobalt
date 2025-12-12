// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Untested Scenarios:
// 1. EFAULT: `path` or `statbuf` points to an invalid memory address.
// 2. EIO: An I/O error occurred while reading from the filesystem.
// 3. ENOMEM: Insufficient kernel memory was available.

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

constexpr char kTestFileContent[] = "Hello, stat!";

class PosixStatTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char template_name[] = "/tmp/stat_test_XXXXXX";
    char* dir_name = mkdtemp(template_name);
    ASSERT_NE(dir_name, nullptr) << "mkdtemp failed: " << strerror(errno);
    test_dir_ = dir_name;

    // Create a file within the test directory.
    file_path_ = test_dir_ + "/the_file.txt";
    int fd = open(file_path_.c_str(), O_CREAT | O_WRONLY, kUserRw);
    ASSERT_NE(fd, -1) << "Failed to create test file: " << strerror(errno);
    ssize_t write_res = write(fd, kTestFileContent, strlen(kTestFileContent));
    ASSERT_EQ(static_cast<unsigned long>(write_res), strlen(kTestFileContent));
    ASSERT_EQ(0, close(fd));

    // Create a symbolic link to the file.
    symlink_path_ = test_dir_ + "/the_symlink";
    ASSERT_EQ(symlink(file_path_.c_str(), symlink_path_.c_str()), 0);
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      RemoveFileOrDirectoryRecursively(test_dir_.c_str());
    }
  }

  std::string test_dir_;
  std::string file_path_;
  std::string symlink_path_;
};

TEST_F(PosixStatTest, SuccessForFile) {
  struct stat statbuf;
  errno = 0;
  ASSERT_EQ(stat(file_path_.c_str(), &statbuf), 0)
      << "stat failed: " << strerror(errno);

  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_FALSE(S_ISDIR(statbuf.st_mode));
  EXPECT_EQ(static_cast<size_t>(statbuf.st_size), strlen(kTestFileContent));
}

TEST_F(PosixStatTest, SuccessForDirectory) {
  struct stat statbuf;
  errno = 0;
  ASSERT_EQ(stat(test_dir_.c_str(), &statbuf), 0)
      << "stat failed: " << strerror(errno);

  EXPECT_TRUE(S_ISDIR(statbuf.st_mode));
  EXPECT_FALSE(S_ISREG(statbuf.st_mode));
}

TEST_F(PosixStatTest, SuccessForSymlink) {
  struct stat statbuf;
  errno = 0;
  // stat() follows symbolic links, so this should describe the target file.
  ASSERT_EQ(stat(symlink_path_.c_str(), &statbuf), 0)
      << "stat failed: " << strerror(errno);

  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_EQ(static_cast<size_t>(statbuf.st_size), strlen(kTestFileContent));
}

// We are not checking st_uid and st_gid since we do not support getuid() and
// getgid().
TEST_F(PosixStatTest, AllFieldsArePopulated) {
  struct stat statbuf;
  time_t current_time = time(NULL);
  ASSERT_NE(current_time, -1);

  errno = 0;
  ASSERT_EQ(stat(file_path_.c_str(), &statbuf), 0)
      << "stat failed: " << strerror(errno);

  // Get parent directory info to compare device ID.
  struct stat parent_statbuf;
  ASSERT_EQ(stat(test_dir_.c_str(), &parent_statbuf), 0);

  // Check device and inode numbers.
  EXPECT_GT(statbuf.st_dev, 0u);
  EXPECT_EQ(statbuf.st_dev, parent_statbuf.st_dev);
  EXPECT_GT(statbuf.st_ino, 0u);

  // Check mode and permissions.
  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_FALSE(S_ISDIR(statbuf.st_mode));
  EXPECT_EQ(statbuf.st_mode & 0777, kUserRw);

  // Check link count
  EXPECT_EQ(statbuf.st_nlink, 1u);

  // Check file size.
  EXPECT_EQ(static_cast<size_t>(statbuf.st_size), strlen(kTestFileContent));

  // Check block size and block count
  // Sanity check, since these can vary by platform.
  EXPECT_GT(statbuf.st_blksize, 0);
  EXPECT_GE(statbuf.st_blocks, 0);

  // Sanity check for timestamps.
  EXPECT_GT(statbuf.st_atim.tv_sec, 0);
  EXPECT_LE(statbuf.st_atim.tv_sec, current_time);
  EXPECT_GT(statbuf.st_mtim.tv_sec, 0);
  EXPECT_LE(statbuf.st_mtim.tv_sec, current_time);
  EXPECT_GT(statbuf.st_ctim.tv_sec, 0);
  EXPECT_LE(statbuf.st_ctim.tv_sec, current_time);

  // Stricter check for timestamps.
  // Access time should be within 1 seconds of the check.
  EXPECT_NEAR(statbuf.st_atim.tv_sec, current_time, 1);
  // Created time should be within 5 seconds of the check.
  EXPECT_NEAR(statbuf.st_ctim.tv_sec, current_time, 5);
  // For a new file, modification time is after (or equal to) created time.
  EXPECT_GE(statbuf.st_mtim.tv_sec, statbuf.st_ctim.tv_sec);
}

TEST_F(PosixStatTest, NonExistentPathFails) {
  struct stat statbuf;
  std::string non_existent_path = test_dir_ + "/does_not_exist";
  errno = 0;
  EXPECT_EQ(stat(non_existent_path.c_str(), &statbuf), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixStatTest, EmptyPathFails) {
  struct stat statbuf;
  errno = 0;
  EXPECT_EQ(stat("", &statbuf), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixStatTest, PathComponentNotDirectoryFails) {
  struct stat statbuf;
  std::string path = file_path_ + "/some_other_dir";
  errno = 0;
  EXPECT_EQ(stat(path.c_str(), &statbuf), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixStatTest, PermissionDeniedFails) {
  // This test will not work correctly if run as root, as root bypasses
  // file permission checks.
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  struct stat statbuf;
  std::string protected_dir = test_dir_ + "/protected";
  ASSERT_EQ(mkdir(protected_dir.c_str(), kUserRwx), 0);

  std::string file_in_protected = protected_dir + "/inner_file";
  int fd = open(file_in_protected.c_str(), O_CREAT | O_WRONLY, kUserRw);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(0, close(fd));

  // Remove search (execute) permission from the directory.
  ASSERT_EQ(chmod(protected_dir.c_str(), kUserRw), 0);

  errno = 0;
  EXPECT_EQ(stat(file_in_protected.c_str(), &statbuf), -1);
  EXPECT_EQ(errno, EACCES);

  // Restore permissions for cleanup.
  chmod(protected_dir.c_str(), kUserRwx);
}

TEST_F(PosixStatTest, PathTooLongFails) {
  struct stat statbuf;
  std::string long_name(kSbFileMaxPath + 1, 'c');
  std::string long_path = test_dir_ + "/" + long_name;

  errno = 0;
  EXPECT_EQ(stat(long_path.c_str(), &statbuf), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST_F(PosixStatTest, SymlinkLoopFails) {
  struct stat statbuf;
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";

  ASSERT_EQ(symlink(link_b_path.c_str(), link_a_path.c_str()), 0);
  ASSERT_EQ(symlink(link_a_path.c_str(), link_b_path.c_str()), 0);

  errno = 0;
  EXPECT_EQ(stat(link_a_path.c_str(), &statbuf), -1);
  EXPECT_EQ(errno, ELOOP);
}

}  // namespace
}  // namespace nplb
