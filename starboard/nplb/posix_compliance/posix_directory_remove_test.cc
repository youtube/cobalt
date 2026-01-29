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

/*
  The following rmdir() error conditions are not tested due to the difficulty
  of reliably triggering them in a portable unit testing environment:

  - [EBUSY]: The directory is in use by the system or another process.
  - [EIO]: A low-level I/O error occurred on the filesystem.
  - [EROFS]: The directory is on a read-only file system.
*/
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

class PosixRmdirTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> temp_path(kSbFileMaxPath);
    bool success = SbSystemGetPath(kSbSystemPathTempDirectory, temp_path.data(),
                                   kSbFileMaxPath);
    ASSERT_TRUE(success);

    std::string path_template =
        std::string(temp_path.data()) + "/rmdir_test.XXXXXX";
    std::vector<char> buffer(path_template.begin(), path_template.end());
    buffer.push_back('\0');

    char* created_path = mkdtemp(buffer.data());
    ASSERT_TRUE(created_path != nullptr);
    test_dir_ = created_path;
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      RemoveFileOrDirectoryRecursively(test_dir_);
    }
  }

  bool DirectoryExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
  }

  std::string test_dir_;
};

TEST_F(PosixRmdirTest, SuccessRemovesEmptyDirectory) {
  std::string dir_path = test_dir_ + "/empty_dir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0);

  ASSERT_TRUE(DirectoryExists(dir_path));
  ASSERT_EQ(rmdir(dir_path.c_str()), 0)
      << "rmdir failed with error: " << strerror(errno);
  EXPECT_FALSE(DirectoryExists(dir_path));
}

TEST_F(PosixRmdirTest, FailsOnNonEmptyDirectory) {
  std::string dir_path = test_dir_ + "/non_empty_dir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0)
      << "mkdir failed with error: " << strerror(errno);

  std::string file_path = dir_path + "/file.txt";
  int fd = open(file_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0) << "close failed with error: " << strerror(errno);

  errno = 0;
  int result = rmdir(dir_path.c_str());
  EXPECT_EQ(result, -1);
  EXPECT_TRUE(errno == EEXIST || errno == ENOTEMPTY);
}

TEST_F(PosixRmdirTest, FailsOnRegularFile) {
  std::string file_path = test_dir_ + "/file.txt";
  int fd = open(file_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0) << "close failed with error: " << strerror(errno);

  errno = 0;
  EXPECT_EQ(rmdir(file_path.c_str()), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixRmdirTest, FailsOnSymbolicLink) {
  std::string target_path = test_dir_ + "/target_dir";
  ASSERT_EQ(mkdir(target_path.c_str(), kUserRwx), 0);

  std::string link_path = test_dir_ + "/link_to_dir";
  ASSERT_EQ(symlink(target_path.c_str(), link_path.c_str()), 0);

  errno = 0;
  EXPECT_EQ(rmdir(link_path.c_str()), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixRmdirTest, FailsWithNoEntry) {
  std::string non_existent_path = test_dir_ + "/no_such_dir";
  errno = 0;
  EXPECT_EQ(rmdir(non_existent_path.c_str()), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixRmdirTest, FailsOnDot) {
  errno = 0;
  EXPECT_EQ(rmdir("."), -1);
  // POSIX specifies EINVAL for ".", but Linux may use EBUSY.
  EXPECT_TRUE(errno == EINVAL || errno == EBUSY);
}

TEST_F(PosixRmdirTest, FailsOnDotDot) {
  errno = 0;
  EXPECT_EQ(rmdir(".."), -1);
  // POSIX doesn't specify the error but it should fail
  EXPECT_NE(errno, 0);
}

TEST_F(PosixRmdirTest, FailsWithPermissionDenied) {
  // This test will not work correctly if run as root, as root bypasses
  // file permission checks.
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  std::string dir_path = test_dir_ + "/subdir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0)
      << "rmkdirmdir failed with error: " << strerror(errno);

  // Remove write permission from the parent directory.
  ASSERT_EQ(chmod(test_dir_.c_str(), S_IRUSR | S_IXUSR), 0)
      << "chmod failed with error: " << strerror(errno);

  errno = 0;
  EXPECT_EQ(rmdir(dir_path.c_str()), -1);
  EXPECT_EQ(errno, EACCES);

  // Restore permissions for cleanup.
  ASSERT_EQ(chmod(test_dir_.c_str(), kUserRwx), 0)
      << "chmod failed with error: " << strerror(errno);
}

TEST_F(PosixRmdirTest, FailsWithNameTooLong) {
  std::string long_name(kSbFileMaxPath + 1, 'a');
  std::string dir_path = test_dir_ + "/" + long_name;

  errno = 0;
  EXPECT_EQ(rmdir(dir_path.c_str()), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST_F(PosixRmdirTest, FailsWithSymbolicLinkLoop) {
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";
  ASSERT_EQ(symlink(link_b_path.c_str(), link_a_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);
  ASSERT_EQ(symlink(link_a_path.c_str(), link_b_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);

  std::string looping_path = link_a_path + "/some_dir";
  errno = 0;
  EXPECT_EQ(rmdir(looping_path.c_str()), -1);
  EXPECT_EQ(errno, ELOOP);
}

}  // namespace
}  // namespace starboard::nplb
